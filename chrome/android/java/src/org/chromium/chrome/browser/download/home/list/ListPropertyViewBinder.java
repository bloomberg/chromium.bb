// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.support.v7.widget.RecyclerView;

import org.chromium.chrome.browser.download.home.list.ListPropertyModel.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor.ViewBinder;

class ListPropertyViewBinder
        implements ViewBinder<ListPropertyModel, RecyclerView, ListPropertyModel.PropertyKey> {
    @Override
    public void bind(ListPropertyModel model, RecyclerView view, PropertyKey propertyKey) {
        if (propertyKey == ListPropertyModel.PropertyKey.ITEM_ANIMATION_DURATION_MS) {
            view.getItemAnimator().setAddDuration(model.getItemAnimationDurationMs());
            view.getItemAnimator().setRemoveDuration(model.getItemAnimationDurationMs());
        } else if (propertyKey == ListPropertyModel.PropertyKey.CALLBACK_OPEN
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_PAUSE
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_RESUME
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_CANCEL
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_SHARE
                || propertyKey == ListPropertyModel.PropertyKey.CALLBACK_REMOVE) {
            view.getAdapter().notifyItemChanged(0, view.getAdapter().getItemCount());
        }
    }
}