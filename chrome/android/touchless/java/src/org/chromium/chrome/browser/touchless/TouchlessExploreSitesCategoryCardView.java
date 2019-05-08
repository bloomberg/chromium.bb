// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.explore_sites.ExploreSitesCategoryCardView;
import org.chromium.chrome.browser.explore_sites.ExploreSitesSite;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.ui.modelutil.PropertyModel;

class TouchlessExploreSitesCategoryCardView extends ExploreSitesCategoryCardView {
    public TouchlessExploreSitesCategoryCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    protected class TouchlessExploreSitesSiteViewBinder
            extends ExploreSitesCategoryCardView.ExploreSitesSiteViewBinder {
        Activity mActivity;
        public TouchlessExploreSitesSiteViewBinder(Activity activity) {
            mActivity = activity;
        }

        @Override
        protected ExploreSitesCategoryCardView.CategoryCardInteractionDelegate
        createInteractionDelegate(PropertyModel model) {
            return new TouchlessCategoryCardInteractionDelegate((ChromeActivity) mActivity, model);
        }
    }

    /**
     * Delegate that's aware of fields necessary for the touchless context menu.
     */
    protected class TouchlessCategoryCardInteractionDelegate
            extends ExploreSitesCategoryCardView.CategoryCardInteractionDelegate
            implements TouchlessContextMenuManager.Delegate {
        private ChromeActivity mActivity;
        private PropertyModel mModel;

        TouchlessCategoryCardInteractionDelegate(ChromeActivity activity, PropertyModel model) {
            super(model.get(ExploreSitesSite.URL_KEY), model.get(ExploreSitesSite.TILE_INDEX_KEY));
            mActivity = activity;
            mModel = model;
        }

        @Override
        public Activity getActivity() {
            return mActivity;
        }

        @Override
        public String getTitle() {
            return mModel.get(ExploreSitesSite.TITLE_KEY);
        }

        @Override
        public Bitmap getIconBitmap() {
            return mModel.get(ExploreSitesSite.ICON_KEY);
        }

        @Override
        public boolean isItemSupported(int menuItemId) {
            return menuItemId == ContextMenuManager.ContextMenuItemId.ADD_TO_MY_APPS
                    || menuItemId == ContextMenuManager.ContextMenuItemId.REMOVE;
        }
    }

    @Override
    protected ExploreSitesSiteViewBinder createViewBinder(Activity activity) {
        return new TouchlessExploreSitesSiteViewBinder(activity);
    }
}
