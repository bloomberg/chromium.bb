// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.load_progress;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.load_progress.LoadProgressProperties.CompletionState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator for the load progress bar. Listens for changes to the loading state of the current tab
 * and adjusts its property model accordingly.
 */
public class LoadProgressMediator {
    static final float MINIMUM_LOAD_PROGRESS = 0.05f;

    private final PropertyModel mModel;
    private final EmptyTabObserver mTabObserver;
    private final LoadProgressSimulator mLoadProgressSimulator;

    public LoadProgressMediator(ActivityTabProvider activityTabProvider, PropertyModel model) {
        mModel = model;
        mLoadProgressSimulator = new LoadProgressSimulator(model);

        mTabObserver = new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider) {
            @Override
            public void onDidStartNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.isSameDocument() || !navigation.isInMainFrame()) {
                    return;
                }

                if (NativePageFactory.isNativePageUrl(navigation.getUrl(), tab.isIncognito())) {
                    finishLoadProgress(false);
                    return;
                }

                mLoadProgressSimulator.cancel();
                startLoadProgress();
                updateLoadProgress(tab.getProgress());
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                if (!toDifferentDocument) return;

                // If we made some progress, fast-forward to complete, otherwise just dismiss any
                // MINIMUM_LOAD_PROGRESS that had been set.
                if (tab.getProgress() > MINIMUM_LOAD_PROGRESS && tab.getProgress() < 1) {
                    updateLoadProgress(1.0f);
                }
                finishLoadProgress(true);
            }

            @Override
            public void onLoadProgressChanged(Tab tab, float progress) {
                if (NewTabPage.isNTPUrl(tab.getUrlString())
                        || NativePageFactory.isNativePageUrl(
                                tab.getUrlString(), tab.isIncognito())) {
                    return;
                }

                updateLoadProgress(progress);
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                // If loading both started and finished before we swapped in the WebContents, we
                // won't get any load progress signals. Otherwise, we should receive at least one
                // real signal so we don't need to simulate them.
                if (didStartLoad && didFinishLoad) {
                    mLoadProgressSimulator.start();
                }
            }

            @Override
            public void onCrash(Tab tab) {
                finishLoadProgress(false);
            }

            @Override
            protected void onObservingDifferentTab(Tab tab) {
                onNewTabObserved(tab);
            }
        };
        onNewTabObserved(activityTabProvider.get());
    }

    private void onNewTabObserved(Tab tab) {
        if (tab == null) return;
        if (tab.isLoading()) {
            if (NativePageFactory.isNativePageUrl(tab.getUrlString(), tab.isIncognito())) {
                finishLoadProgress(false);
            } else {
                startLoadProgress();
                updateLoadProgress(tab.getProgress());
            }
        } else {
            finishLoadProgress(false);
        }
    }

    private void startLoadProgress() {
        mModel.set(LoadProgressProperties.COMPLETION_STATE, CompletionState.UNFINISHED);
    }

    private void updateLoadProgress(float progress) {
        progress = Math.max(progress, MINIMUM_LOAD_PROGRESS);
        mModel.set(LoadProgressProperties.PROGRESS, progress);
        if (MathUtils.areFloatsEqual(progress, 1)) finishLoadProgress(true);
    }

    private void finishLoadProgress(boolean animateCompletion) {
        mLoadProgressSimulator.cancel();
        @CompletionState
        int completionState = animateCompletion ? CompletionState.FINISHED_DO_ANIMATE
                                                : CompletionState.FINISHED_DONT_ANIMATE;
        mModel.set(LoadProgressProperties.COMPLETION_STATE, completionState);
    }
}
