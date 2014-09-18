// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.text.TextUtils;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content_public.common.Referrer;

import java.util.ArrayList;

/**
 * A list of parameters that explain what kind of context menu to show the user.  This data is
 * generated from content/public/common/context_menu_params.h.
 */
@JNINamespace("ContextMenuParamsAndroid")
public class ContextMenuParams {
    /** Must correspond to the MediaType enum in WebKit/chromium/public/WebContextMenuData.h */
    @SuppressWarnings("unused")
    private static interface MediaType {
        public static final int MEDIA_TYPE_NONE = 0;
        public static final int MEDIA_TYPE_IMAGE = 1;
        public static final int MEDIA_TYPE_VIDEO = 2;
        public static final int MEDIA_TYPE_AUDIO = 3;
        public static final int MEDIA_TYPE_FILE = 4;
        public static final int MEDIA_TYPE_PLUGIN = 5;
    }

    private static class CustomMenuItem {
        public final String label;
        public final int action;

        public CustomMenuItem(String label, int action) {
            this.label = label;
            this.action = action;
        }
    }

    private final String mLinkUrl;
    private final String mLinkText;
    private final String mUnfilteredLinkUrl;
    private final String mSrcUrl;
    private final boolean mIsEditable;
    private final Referrer mReferrer;

    private final boolean mIsAnchor;
    private final boolean mIsSelectedText;
    private final boolean mIsImage;
    private final boolean mIsVideo;

    private final ArrayList<CustomMenuItem> mCustomMenuItems = new ArrayList<CustomMenuItem>();

    /**
     * @return Whether or not the context menu should consist of custom items.
     */
    public boolean isCustomMenu() {
        return !mCustomMenuItems.isEmpty();
    }

    /**
     * @return The number of custom items in this context menu.
     */
    public int getCustomMenuSize() {
        return mCustomMenuItems.size();
    }

    /**
     * The label that should be shown for the custom menu item at {@code index}.
     * @param index The index of the custom menu item.
     * @return      The label to show.
     */
    public String getCustomLabelAt(int index) {
        assert index >= 0 && index < mCustomMenuItems.size();
        return mCustomMenuItems.get(index).label;
    }

    /**
     * The action that should be returned for the custom menu item at {@code index}.
     * @param index The index of the custom menu item.
     * @return      The action to return.
     */
    public int getCustomActionAt(int index) {
        assert index >= 0 && index < mCustomMenuItems.size();
        return mCustomMenuItems.get(index).action;
    }

    /**
     * @return The link URL, if any.
     */
    public String getLinkUrl() {
        return mLinkUrl;
    }

    /**
     * @return The link text, if any.
     */
    public String getLinkText() {
        return mLinkText;
    }

    /**
     * @return The unfiltered link URL, if any.
     */
    public String getUnfilteredLinkUrl() {
        return mUnfilteredLinkUrl;
    }

    /**
     * @return The source URL.
     */
    public String getSrcUrl() {
        return mSrcUrl;
    }

    /**
     * @return Whether or not the context menu is being shown for an editable piece of content.
     */
    public boolean isEditable() {
        return mIsEditable;
    }

    /**
     * @return the referrer associated with the frame on which the menu is invoked
     */
    public Referrer getReferrer() {
        return mReferrer;
    }

    /**
     * @return Whether or not the context menu is being shown for an anchor.
     */
    public boolean isAnchor() {
        return mIsAnchor;
    }

    /**
     * @return Whether or not the context menu is being shown for selected text.
     */
    public boolean isSelectedText() {
        return mIsSelectedText;
    }

    /**
     * @return Whether or not the context menu is being shown for an image.
     */
    public boolean isImage() {
        return mIsImage;
    }

    /**
     * @return Whether or not the context menu is being shown for a video.
     */
    public boolean isVideo() {
        return mIsVideo;
    }

    private ContextMenuParams(int mediaType, String linkUrl, String linkText,
            String unfilteredLinkUrl, String srcUrl, String selectionText, boolean isEditable,
            Referrer referrer) {
        mLinkUrl = linkUrl;
        mLinkText = linkText;
        mUnfilteredLinkUrl = unfilteredLinkUrl;
        mSrcUrl = srcUrl;
        mIsEditable = isEditable;
        mReferrer = referrer;

        mIsAnchor = !TextUtils.isEmpty(linkUrl);
        mIsSelectedText = !TextUtils.isEmpty(selectionText);
        mIsImage = mediaType == MediaType.MEDIA_TYPE_IMAGE;
        mIsVideo = mediaType == MediaType.MEDIA_TYPE_VIDEO;
    }

    @CalledByNative
    private static ContextMenuParams create(int mediaType, String linkUrl, String linkText,
            String unfilteredLinkUrl, String srcUrl, String selectionText, boolean isEditable,
            String sanitizedReferrer, int referrerPolicy) {
        Referrer referrer = TextUtils.isEmpty(sanitizedReferrer) ?
                null : new Referrer(sanitizedReferrer, referrerPolicy);
        return new ContextMenuParams(mediaType, linkUrl, linkText, unfilteredLinkUrl, srcUrl,
                selectionText, isEditable, referrer);
    }

    @CalledByNative
    private void addCustomItem(String label, int action) {
        mCustomMenuItems.add(new CustomMenuItem(label, action));
    }
}
