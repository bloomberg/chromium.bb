// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.content.browser.LoadUrlParams;

import java.util.List;

/**
 * TabModelSelector is a wrapper class containing both a normal and an incognito TabModel.
 * This class helps the app know which mode it is currently in, and which TabModel it should
 * be using.
 */
public interface TabModelSelector {
    /**
     * Interface of listener that get notified on changes in the {@link TabModel}s.
     */
    public interface ChangeListener {
        /**
         * Called whenever the {@link TabModel} has changed.
         */
        void onChange();

        /**
         * Called when a new tab is created.
         */
        void onNewTabCreated(Tab tab);
    }

    /**
     * Set the current model. This won't cause an animation, but will still change the stack that is
     * currently visible if the tab switcher is open.
     */
    void selectModel(boolean incognito);

    /**
     * Get a specific tab model
     * @return Never returns null.  Returns a stub when real model is uninitialized.
     */
    TabModel getModel(boolean incognito);

    /**
     * @return a list for the underlying models
     */
    List<TabModel> getModels();

    /**
     * @return the model at {@code index} or null if no model exist for that index.
     */
    TabModel getModelAt(int index);

    /**
     * Get the current tab model.
     * @return Never returns null.  Returns a stub when real model is uninitialized.
     */
    TabModel getCurrentModel();

    /**
     * Convenience function to get the current tab on the current model
     * @return Current tab or null if none exists or if the model is not initialized.
     */
    Tab getCurrentTab();

    /**
     * Convenience function to get the current tab id on the current model.
     * @return Id of the current tab or {@link Tab#INVALID_TAB_ID} if no tab is selected or the
     *         model is not initialized.
     */
    int getCurrentTabId();

    /**
     * Convenience function to get the {@link TabModel} for a {@link Tab} specified by
     * {@code id}.
     * @param id The id of the {@link Tab} to find the {@link TabModel} for.
     * @return   The {@link TabModel} that owns the {@link Tab} specified by {@code id}.
     */
    TabModel getModelForTabId(int id);

    /**
     * @return The index of the current {@link TabModel}.
     */
    int getCurrentModelIndex();

    /**
     * @return If the incognito {@link TabModel} is current.
     */
    boolean isIncognitoSelected();

    /**
     * Opens a new tab.
     *
     * @param loadUrlParams parameters of the url load
     * @param type Describes how the new tab is being opened.
     * @param parent The parent tab for this new tab (or null if one does not exist).
     * @param incognito Whether to open the new tab in incognito mode.
     * @return The newly opened tab.
     */
    Tab openNewTab(LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent, boolean incognito);

    /**
     * Searches through all children models for the specified Tab and closes the tab if it exists.
     * @param tab the non-null tab to close
     * @return true if the tab was found
     */
    boolean closeTab(Tab tab);

    /**
     * Close all tabs across all tab models
     */
    void closeAllTabs();

    /**
     * Get total tab count across all tab models
     */
    int getTotalTabCount();

    /**
     * Search all TabModels for a tab with the specified id.
     * @return specified tab or null if tab is not found
     */
    Tab getTabById(int id);

    /**
     * Registers a listener that get notified when the {@link TabModel} changes. Multiple listeners
     * can be registered at the same time.
     * @param changeListener The {@link TabModelSelector.ChangeListener} to notify.
     */
    void registerChangeListener(ChangeListener changeListener);

    /**
     * Unregisters the listener.
     * @param changeListener The {@link TabModelSelector.ChangeListener} to remove.
     */
    void unregisterChangeListener(ChangeListener changeListener);

    /**
     * Calls {@link TabModel#commitAllTabClosures()} on all {@link TabModel}s owned by this
     * selector.
     */
    void commitAllTabClosures();
}
