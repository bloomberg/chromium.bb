// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewStub;

import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.touchless.R;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

import java.util.Date;

/**
 * Controller for the carousel version of most likely, to be shown on touchless devices.
 */
public class SiteSuggestionsController {
    static final PropertyModel.WritableIntPropertyKey CURRENT_INDEX_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel
            .ReadableObjectPropertyKey<ListModel<SiteSuggestion>> SUGGESTIONS_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();

    private View mSuggestionsView;
    private PropertyModel mModel;

    public SiteSuggestionsController(View parentView) {
        mSuggestionsView = ((ViewStub) parentView.findViewById(R.id.most_likely_stub)).inflate();
        mModel = new PropertyModel.Builder(CURRENT_INDEX_KEY, SUGGESTIONS_KEY)
                         .with(CURRENT_INDEX_KEY, Integer.MAX_VALUE / 2)
                         .with(SUGGESTIONS_KEY, new ListModel<>())
                         .build();

        // TODO(chili): Remove when we add suggestions fetching.
        mModel.get(SUGGESTIONS_KEY)
                .add(new SiteSuggestion(
                        "All Apps", "http://www.example.com", "", 0, 1, 2, new Date()));
        mModel.get(SUGGESTIONS_KEY)
                .add(new SiteSuggestion("Fie", "http://www.example.com", "", 0, 1, 2, new Date()));
        mModel.get(SUGGESTIONS_KEY)
                .add(new SiteSuggestion("Fi", "http://www.example.com", "", 0, 1, 2, new Date()));
        mModel.get(SUGGESTIONS_KEY)
                .add(new SiteSuggestion("Fo", "http://www.example.com", "", 0, 1, 2, new Date()));
        mModel.get(SUGGESTIONS_KEY)
                .add(new SiteSuggestion("Fum", "http://www.example.com", "", 0, 1, 2, new Date()));

        Context context = parentView.getContext();

        RoundedIconGenerator iconGenerator = ViewUtils.createDefaultRoundedIconGenerator(
                context.getResources(), /* circularIcon = */ true);

        LinearLayoutManager layoutManager = new LinearLayoutManager(
                context, LinearLayoutManager.HORIZONTAL, /* reverseLayout= */ false);
        RecyclerView recyclerView =
                mSuggestionsView.findViewById(R.id.most_likely_launcher_recycler);
        SiteSuggestionsAdapter adapterDelegate =
                new SiteSuggestionsAdapter(mModel, iconGenerator, context);

        RecyclerViewAdapter<SiteSuggestionsViewHolderFactory.SiteSuggestionsViewHolder, Void>
                adapter = new RecyclerViewAdapter<>(
                        adapterDelegate, new SiteSuggestionsViewHolderFactory());

        recyclerView.setLayoutManager(layoutManager);
        recyclerView.setAdapter(adapter);
    }
}