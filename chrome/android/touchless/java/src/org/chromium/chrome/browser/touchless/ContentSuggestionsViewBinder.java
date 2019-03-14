// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

final class ContentSuggestionsViewBinder {
    public static void bind(
            PropertyModel model, SuggestionsRecyclerView view, PropertyKey propertyKey) {
        if (TouchlessNewTabPageProperties.SCROLL_POSITION_CALLBACK == propertyKey) {
            Callback<Integer> callback =
                    model.get(TouchlessNewTabPageProperties.SCROLL_POSITION_CALLBACK);
            view.addOnScrollListener(new RecyclerView.OnScrollListener() {
                @Override
                public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                    super.onScrolled(recyclerView, dx, dy);
                    callback.onResult(((LinearLayoutManager) recyclerView.getLayoutManager())
                                              .findFirstVisibleItemPosition());
                }
            });
        } else if (TouchlessNewTabPageProperties.INITIAL_SCROLL_POSITION == propertyKey) {
            view.getLinearLayoutManager().scrollToPosition(
                    model.get(TouchlessNewTabPageProperties.INITIAL_SCROLL_POSITION));
        }
    }
}
