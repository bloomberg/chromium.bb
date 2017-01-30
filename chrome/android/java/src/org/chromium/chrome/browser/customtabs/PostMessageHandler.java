// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.net.Uri;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.customtabs.PostMessageServiceConnection;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.AppWebMessagePort;
import org.chromium.content.browser.AppWebMessagePortService;
import org.chromium.content.browser.AppWebMessagePortService.MessageChannelObserver;
import org.chromium.content.browser.PostMessageSender;
import org.chromium.content.browser.PostMessageSender.PostMessageSenderDelegate;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.MessagePort.MessageCallback;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * A class that handles postMessage communications with a designated {@link CustomTabsSessionToken}.
 */
public class PostMessageHandler extends PostMessageServiceConnection {
    private static AppWebMessagePortService sService;

    private final MessageCallback mMessageCallback;
    private WebContents mWebContents;
    private boolean mMessageChannelCreated;
    private boolean mBoundToService;
    private AppWebMessagePort[] mChannel;
    private PostMessageSender mPostMessageSender;
    private PostMessageSenderDelegate mSenderDelegate;
    private Uri mOrigin;

    private static AppWebMessagePortService getAppWebMessagePortService() {
        if (sService == null) sService = new AppWebMessagePortService();
        return sService;
    }

    /**
     * Basic constructor. Everytime the given {@link CustomTabsSessionToken} is associated with a
     * new {@link WebContents},
     * {@link PostMessageHandler#reset(WebContents)} should be called to
     * reset all internal state.
     * @param session The {@link CustomTabsSessionToken} to establish the postMessage communication
     *                with.
     */
    public PostMessageHandler(CustomTabsSessionToken session) {
        super(session);
        mMessageCallback = new MessageCallback() {
            @Override
            public void onMessage(String message, MessagePort[] sentPorts) {
                if (mBoundToService) postMessage(message, null);
            }
        };
    }

    /**
     * Resets the internal state of the handler, linking the associated
     * {@link CustomTabsSessionToken} with a new {@link WebContents} and the {@link Tab} that
     * contains it.
     * @param webContents The new {@link WebContents} that the session got associated with. If this
     *                    is null, the handler disconnects and unbinds from service.
     */
    public void reset(final WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) {
            disconnectChannel();
            unbindFromContext(ContextUtils.getApplicationContext());
            return;
        }
        // Can't reset with the same web contents twice.
        if (webContents.equals(mWebContents)) return;
        mWebContents = webContents;
        if (mOrigin == null) return;
        new WebContentsObserver(webContents) {
            private boolean mNavigatedOnce;

            @Override
            public void didNavigateMainFrame(String url, String baseUrl,
                    boolean isNavigationToDifferentPage, boolean isFragmentNavigation,
                    int statusCode) {
                if (mNavigatedOnce && isNavigationToDifferentPage && mChannel != null) {
                    webContents.removeObserver(this);
                    disconnectChannel();
                    unbindFromContext(ContextUtils.getApplicationContext());
                    return;
                }
                mNavigatedOnce = true;
            }

            @Override
            public void renderProcessGone(boolean wasOomProtected) {
                disconnectChannel();
                unbindFromContext(ContextUtils.getApplicationContext());
            }

            @Override
            public void documentLoadedInFrame(long frameId, boolean isMainFrame) {
                if (!isMainFrame || mChannel != null) return;
                initializeWithWebContents(webContents);
            }
        };
    }

    private void initializeWithWebContents(final WebContents webContents) {
        final AppWebMessagePortService service = getAppWebMessagePortService();
        mChannel = (AppWebMessagePort[]) webContents.createMessageChannel(service);
        mChannel[0].setMessageCallback(mMessageCallback, null);
        mSenderDelegate = new PostMessageSenderDelegate() {
            @Override
            public void postMessageToWeb(
                    String frameName, String message, String targetOrigin,
                    int[] sentPortIds) {
                if (webContents.isDestroyed()) {
                    disconnectChannel();
                    unbindFromContext(ContextUtils.getApplicationContext());
                    return;
                }
                webContents.postMessageToFrame(
                        frameName, message, mOrigin.toString(), targetOrigin, sentPortIds);
            }

            @Override
            public void onPostMessageQueueEmpty() {}

            @Override
            public boolean isPostMessageSenderReady() {
                return true;
            }
        };
        mPostMessageSender = new PostMessageSender(
                mSenderDelegate, getAppWebMessagePortService());
        service.addObserver(new MessageChannelObserver() {
            @Override
            public void onMessageChannelCreated() {
                service.removeObserver(this);
                if (mChannel == null) return;
                mPostMessageSender.postMessage(
                        null, "", "", new AppWebMessagePort[] {mChannel[1]});
                mMessageChannelCreated = true;
                if (mBoundToService) notifyMessageChannelReady(null);
            }
        });
    }

    private void disconnectChannel() {
        if (mChannel == null) return;
        mChannel[0].close();
        mChannel = null;
        mSenderDelegate = null;
        mPostMessageSender = null;
        mWebContents = null;
    }

    /**
     * Sets the postMessage origin for this session to the given {@link Uri}.
     * @param origin The origin value to be set.
     */
    public void initializeWithOrigin(Uri origin) {
        mOrigin = origin;
        if (mWebContents != null && !mWebContents.isDestroyed()) {
            initializeWithWebContents(mWebContents);
        }
    }

    /**
     * Relay a postMessage request through the current channel assigned to this session.
     * @param message The message to be sent.
     * @return The result of the postMessage request. Returning true means the request was accepted,
     *         not necessarily that the postMessage was successful.
     */
    public int postMessageFromClientApp(final String message) {
        if (mChannel == null || !mChannel[0].isReady() || mChannel[0].isClosed()) {
            return CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR;
        }
        if (mWebContents == null || mWebContents.isDestroyed()) {
            return CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR;
        }
        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                // It is still possible that the page has navigated while this task is in the queue.
                // If that happens fail gracefully.
                if (mChannel == null || mChannel[0].isClosed()) return;
                mChannel[0].postMessage(message, null);
            }
        });
        return CustomTabsService.RESULT_SUCCESS;
    }

    @Override
    public void unbindFromContext(Context context) {
        if (mBoundToService) super.unbindFromContext(context);
    }

    @Override
    public void onPostMessageServiceConnected() {
        mBoundToService = true;
        if (mMessageChannelCreated) notifyMessageChannelReady(null);
    }

    @Override
    public void onPostMessageServiceDisconnected() {
        mBoundToService = false;
    }
}
