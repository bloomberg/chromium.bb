// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.content.browser.accessibility.WebContentsAccessibilityImpl;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.input.SelectPopup;
import org.chromium.content.browser.input.TextSuggestionHost;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ContentViewCore;
import org.chromium.content_public.browser.ViewEventSink.InternalAccessDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Implementation of the interface {@ContentViewCore}.
 */
public class ContentViewCoreImpl implements ContentViewCore {
    private static final String TAG = "cr_ContentViewCore";

    private WebContentsImpl mWebContents;

    private boolean mInitialized;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<ContentViewCoreImpl> INSTANCE =
                ContentViewCoreImpl::new;
    }

    /**
     * @param webContents The {@link WebContents} to find a {@link ContentViewCore} of.
     * @return            A {@link ContentViewCore} that is connected to {@code webContents} or
     *                    {@code null} if none exists.
     */
    public static ContentViewCoreImpl fromWebContents(WebContents webContents) {
        return webContents.getOrSetUserData(ContentViewCoreImpl.class, null);
    }

    /**
     * Constructs a new ContentViewCore.
     *
     * @param webContents {@link WebContents} to be associated with this object.
     */
    public ContentViewCoreImpl(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
    }

    public static ContentViewCoreImpl create(Context context, String productVersion,
            WebContents webContents, ViewAndroidDelegate viewDelegate,
            InternalAccessDelegate internalDispatcher, WindowAndroid windowAndroid) {
        ContentViewCoreImpl core = webContents.getOrSetUserData(
                ContentViewCoreImpl.class, UserDataFactoryLazyHolder.INSTANCE);
        assert core != null;
        assert !core.initialized();
        core.initialize(context, productVersion, viewDelegate, internalDispatcher, windowAndroid);
        return core;
    }

    private void initialize(Context context, String productVersion,
            ViewAndroidDelegate viewDelegate, InternalAccessDelegate internalDispatcher,
            WindowAndroid windowAndroid) {
        // Ensure ViewEventSinkImpl is initialized first before being accessed by
        // WindowEventObserverManagerImpl via WebContentsImpl.
        ViewEventSinkImpl.create(context, mWebContents);

        mWebContents.setViewAndroidDelegate(viewDelegate);
        mWebContents.setTopLevelNativeWindow(windowAndroid);

        ViewGroup containerView = viewDelegate.getContainerView();
        ImeAdapterImpl.create(
                mWebContents, ImeAdapterImpl.createDefaultInputMethodManagerWrapper(context));
        SelectionPopupControllerImpl.create(context, windowAndroid, mWebContents);
        WebContentsAccessibilityImpl.create(context, containerView, mWebContents, productVersion);
        TapDisambiguator.create(context, mWebContents, containerView);
        TextSuggestionHost.create(context, mWebContents, windowAndroid);
        SelectPopup.create(context, mWebContents);
        Gamepad.create(context, mWebContents);

        ViewEventSinkImpl.from(mWebContents).setAccessDelegate(internalDispatcher);
        mWebContents.getRenderCoordinates().setDeviceScaleFactor(
                windowAndroid.getDisplay().getDipScale());

        mInitialized = true;
    }

    private boolean initialized() {
        return mInitialized;
    }
}
