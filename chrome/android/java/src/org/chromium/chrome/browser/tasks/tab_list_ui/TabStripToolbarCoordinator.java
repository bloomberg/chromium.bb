// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_list_ui;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.ACTIVITY_CONTEXT;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import javax.inject.Named;

/**
 * A coordinator for BottomTabStripToolbar component.
 */
public class TabStripToolbarCoordinator implements Destroyable {
    private final BottomTabListToolbarView mToolbarView;
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mModelChangeProcessor;

    TabStripToolbarCoordinator(
            @Named(ACTIVITY_CONTEXT) Context context, ViewGroup parentView, PropertyModel model) {
        mModel = model;
        mToolbarView = (BottomTabListToolbarView) LayoutInflater.from(context).inflate(
                R.layout.bottom_tab_strip_toolbar, parentView, false);

        parentView.addView(mToolbarView);

        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                model, mToolbarView, BottomTabStripToolbarViewBinder::bind);
    }

    View getView() {
        return mToolbarView;
    }

    ViewGroup getTabListContainerView() {
        return mToolbarView.getViewContainer();
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        mModelChangeProcessor.destroy();
    }
}
