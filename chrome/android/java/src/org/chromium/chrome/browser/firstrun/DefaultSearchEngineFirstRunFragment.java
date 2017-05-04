// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Fragment;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.locale.DefaultSearchEngineDialogHelper;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.widget.RadioButtonLayout;

/** A {@link Fragment} that presents a set of search engines for the user to choose from. */
public class DefaultSearchEngineFirstRunFragment
        extends FirstRunPage implements TemplateUrlService.LoadListener {
    private DefaultSearchEngineDialogHelper mHelper;

    /** Layout that displays the available search engines to the user. */
    private RadioButtonLayout mEngineLayout;

    /** The button that lets a user proceed to the next page after an engine is selected. */
    private Button mButton;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View rootView = inflater.inflate(
                R.layout.default_search_engine_first_run_fragment, container, false);
        mEngineLayout = (RadioButtonLayout) rootView.findViewById(R.id.engine_controls);
        mButton = (Button) rootView.findViewById(R.id.button_primary);
        mButton.setEnabled(false);

        TemplateUrlService.getInstance().registerLoadListener(this);
        if (!TemplateUrlService.getInstance().isLoaded()) TemplateUrlService.getInstance().load();

        return rootView;
    }

    @Override
    public void onTemplateUrlServiceLoaded() {
        TemplateUrlService.getInstance().unregisterLoadListener(this);

        int dialogType = LocaleManager.getInstance().getSearchEnginePromoShowType();
        Runnable dismissRunnable = new Runnable() {
            @Override
            public void run() {
                advanceToNextPage();
            }
        };
        mHelper = new DefaultSearchEngineDialogHelper(
                dialogType, mEngineLayout, mButton, dismissRunnable);
    }
}
