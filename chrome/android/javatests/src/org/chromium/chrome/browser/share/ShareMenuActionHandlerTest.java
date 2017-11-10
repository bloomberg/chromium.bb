// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.annotation.SuppressLint;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Parcel;
import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.content_public.browser.AccessibilitySnapshotCallback;
import org.chromium.content_public.browser.ContentBitmapCallback;
import org.chromium.content_public.browser.ImageDownloadCallback;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.services.service_manager.InterfaceProvider;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Annotation;
import java.util.Map;
import java.util.concurrent.ExecutionException;

/**
 * Tests (requiring native) of the ShareMenuActionHandler.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ShareMenuActionHandlerTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Test
    @SmallTest
    public void testShouldFetchCanonicalUrl() throws ExecutionException {
        MockTab mockTab = ThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(); });
        MockWebContents mockWebContents = new MockWebContents();
        MockRenderFrameHost mockRenderFrameHost = new MockRenderFrameHost();

        // Null webContents:
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));

        // Null render frame:
        mockTab.webContents = mockWebContents;
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));

        // Invalid/empty URL:
        mockWebContents.renderFrameHost = mockRenderFrameHost;
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));

        // Insecure URL:
        mockTab.url = "http://www.example.com";
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));

        // Valid URL!
        mockTab.url = "https://www.example.com";
        Assert.assertTrue(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));

        // Disabled if showing error page.
        mockTab.isShowingErrorPage = true;
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));
        mockTab.isShowingErrorPage = false;

        // Disabled if showing interstitial page.
        mockTab.isShowingInterstitialPage = true;
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));
        mockTab.isShowingInterstitialPage = false;

        // Disabled if showing sad tab page.
        mockTab.isShowingSadTab = true;
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));
        mockTab.isShowingSadTab = false;
    }

    @Test
    @SmallTest
    public void testGetUrlToShare() {
        Assert.assertNull(ShareMenuActionHandler.getUrlToShare(null, null));
        Assert.assertEquals("", ShareMenuActionHandler.getUrlToShare("", null));

        final String httpUrl = "http://blah.com";
        final String otherHttpUrl = "http://eek.com";
        final String httpsUrl = "https://blah.com";
        final String otherHttpsUrl = "https://eek.com";

        // HTTP, HTTP
        Assert.assertEquals(httpUrl, ShareMenuActionHandler.getUrlToShare(httpUrl, otherHttpUrl));
        // HTTP, HTTPS
        Assert.assertEquals(httpUrl, ShareMenuActionHandler.getUrlToShare(httpUrl, httpsUrl));
        // HTTPS, HTTP
        Assert.assertEquals(httpsUrl, ShareMenuActionHandler.getUrlToShare(httpsUrl, httpUrl));

        Assert.assertEquals(httpsUrl, ShareMenuActionHandler.getUrlToShare(httpsUrl, httpsUrl));
        Assert.assertEquals(
                otherHttpsUrl, ShareMenuActionHandler.getUrlToShare(httpsUrl, otherHttpsUrl));
    }

    private static class MockTab extends Tab {
        public WebContents webContents;
        public String url;
        public boolean isShowingErrorPage;
        public boolean isShowingInterstitialPage;
        public boolean isShowingSadTab;

        public MockTab() {
            super(INVALID_TAB_ID, false, null);
        }

        @Override
        public String getUrl() {
            return url;
        }

        @Override
        public WebContents getWebContents() {
            return webContents;
        }

        @Override
        public boolean isShowingErrorPage() {
            return isShowingErrorPage;
        }

        @Override
        public boolean isShowingInterstitialPage() {
            return isShowingInterstitialPage;
        }

        @Override
        public boolean isShowingSadTab() {
            return isShowingSadTab;
        }
    }

    @SuppressLint("ParcelCreator")
    private static class MockWebContents implements WebContents {
        public RenderFrameHost renderFrameHost;

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {}

        @Override
        public void setInternalsHolder(InternalsHolder holder) {}

        @Override
        public WindowAndroid getTopLevelNativeWindow() {
            return null;
        }

        @Override
        public void destroy() {}

        @Override
        public boolean isDestroyed() {
            return false;
        }

        @Override
        public NavigationController getNavigationController() {
            return null;
        }

        @Override
        public RenderFrameHost getMainFrame() {
            return renderFrameHost;
        }

        @Override
        public String getTitle() {
            return null;
        }

        @Override
        public String getVisibleUrl() {
            return null;
        }

        @Override
        public String getEncoding() {
            return null;
        }

        @Override
        public boolean isLoading() {
            return false;
        }

        @Override
        public boolean isLoadingToDifferentDocument() {
            return false;
        }

        @Override
        public void stop() {}

        @Override
        public void cut() {}

        @Override
        public void copy() {}

        @Override
        public void paste() {}

        @Override
        public void pasteAsPlainText() {}

        @Override
        public void replace(String word) {}

        @Override
        public void selectAll() {}

        @Override
        public void collapseSelection() {}

        @Override
        public void onHide() {}

        @Override
        public void onShow() {}

        @Override
        public void setImportance(int importance) {}

        @Override
        public void dismissTextHandles() {}

        @Override
        public void showContextMenuAtTouchHandle(int x, int y) {}

        @Override
        public void suspendAllMediaPlayers() {}

        @Override
        public void setAudioMuted(boolean mute) {}

        @Override
        public int getBackgroundColor() {
            return 0;
        }

        @Override
        public void showInterstitialPage(String url, long interstitialPageDelegateAndroid) {}

        @Override
        public boolean isShowingInterstitialPage() {
            return false;
        }

        @Override
        public boolean focusLocationBarByDefault() {
            return false;
        }

        @Override
        public boolean isReady() {
            return false;
        }

        @Override
        public void exitFullscreen() {}

        @Override
        public void updateBrowserControlsState(
                boolean enableHiding, boolean enableShowing, boolean animate) {}

        @Override
        public void scrollFocusedEditableNodeIntoView() {}

        @Override
        public void selectWordAroundCaret() {}

        @Override
        public void adjustSelectionByCharacterOffset(
                int startAdjust, int endAdjust, boolean showSelectionMenu) {}

        @Override
        public String getLastCommittedUrl() {
            return null;
        }

        @Override
        public boolean isIncognito() {
            return false;
        }

        @Override
        public void resumeLoadingCreatedWebContents() {}

        @Override
        public void evaluateJavaScript(String script, JavaScriptCallback callback) {}

        @Override
        public void evaluateJavaScriptForTests(String script, JavaScriptCallback callback) {}

        @Override
        public void addMessageToDevToolsConsole(int level, String message) {}

        @Override
        public void postMessageToFrame(String frameName, String message, String sourceOrigin,
                String targetOrigin, MessagePort[] ports) {}

        @Override
        public MessagePort[] createMessageChannel() {
            return null;
        }

        @Override
        public boolean hasAccessedInitialDocument() {
            return false;
        }

        @Override
        public int getThemeColor() {
            return 0;
        }

        @Override
        public void requestSmartClipExtract(int x, int y, int width, int height) {}

        @Override
        public void setSmartClipResultHandler(Handler smartClipHandler) {}

        @Override
        public void requestAccessibilitySnapshot(AccessibilitySnapshotCallback callback) {}

        @Override
        public EventForwarder getEventForwarder() {
            return null;
        }

        @Override
        public void addObserver(WebContentsObserver observer) {}

        @Override
        public void removeObserver(WebContentsObserver observer) {}

        @Override
        public void setOverscrollRefreshHandler(OverscrollRefreshHandler handler) {}

        @Override
        public void getContentBitmapAsync(int width, int height, ContentBitmapCallback callback) {}

        @Override
        public void reloadLoFiImages() {}

        @Override
        public int downloadImage(String url, boolean isFavicon, int maxBitmapSize,
                boolean bypassCache, ImageDownloadCallback callback) {
            return 0;
        }

        @Override
        public boolean hasActiveEffectivelyFullscreenVideo() {
            return false;
        }

        @Override
        public Rect getFullscreenVideoSize() {
            return null;
        }

        @Override
        public void simulateRendererKilledForTesting(boolean wasOomProtected) {}

        @Override
        public void setHasPersistentVideo(boolean value) {}

        @Override
        public void setSize(int width, int height) {}

        @Override
        public Map<String, Pair<Object, Class>> getJavascriptInterfaces() {
            return null;
        }

        @Override
        public void setAllowJavascriptInterfacesInspection(boolean allow) {}

        @Override
        public void addPossiblyUnsafeJavascriptInterface(
                Object object, String name, Class<? extends Annotation> requiredAnnotation) {}

        @Override
        public void removeJavascriptInterface(String name) {}
    }

    private static class MockRenderFrameHost implements RenderFrameHost {
        @Override
        public String getLastCommittedURL() {
            return null;
        }

        @Override
        public void getCanonicalUrlForSharing(Callback<String> callback) {}

        @Override
        public InterfaceProvider getRemoteInterfaces() {
            return null;
        }
    }
}
