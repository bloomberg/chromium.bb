// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.Tab;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Implement methods shared across the different model implementations.
 */
public abstract class TabModelSelectorBase implements TabModelSelector {
    public static final int NORMAL_TAB_MODEL_INDEX = 0;
    public static final int INCOGNITO_TAB_MODEL_INDEX = 1;

    private List<TabModel> mTabModels = Collections.emptyList();
    private int mActiveModelIndex = NORMAL_TAB_MODEL_INDEX;
    private final ArrayList<ChangeListener> mChangeListeners = new ArrayList<ChangeListener>();

    protected final void initialize(boolean startIncognito, TabModel... models) {
        // Only normal and incognito supported for now.
        assert mTabModels.isEmpty();
        assert models.length > 0;
        if (startIncognito) {
            assert models.length > INCOGNITO_TAB_MODEL_INDEX;
        }

        List<TabModel> tabModels = new ArrayList<TabModel>();
        for (int i = 0; i < models.length; i++) {
            tabModels.add(models[i]);
        }
        mActiveModelIndex = startIncognito ? INCOGNITO_TAB_MODEL_INDEX : NORMAL_TAB_MODEL_INDEX;
        mTabModels = Collections.unmodifiableList(tabModels);
    }

    @Override
    public void selectModel(boolean incognito) {
        mActiveModelIndex = incognito ? INCOGNITO_TAB_MODEL_INDEX : NORMAL_TAB_MODEL_INDEX;
    }

    @Override
    public TabModel getModelAt(int index) {
        assert (index < mTabModels.size() && index >= 0) :
            "requested index " + index + " size " + mTabModels.size();
        return mTabModels.get(index);
    }

    @Override
    public Tab getCurrentTab() {
        return getCurrentModel() == null ? null : TabModelUtils.getCurrentTab(getCurrentModel());
    }

    @Override
    public int getCurrentTabId() {
        Tab tab = getCurrentTab();
        return tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
    }

    @Override
    public TabModel getModelForTabId(int id) {
        for (int i = 0; i < mTabModels.size(); i++) {
            TabModel model = mTabModels.get(i);
            if (TabModelUtils.getTabById(model, id) != null || model.isClosurePending(id)) {
                return model;
            }
        }
        return null;
    }

    @Override
    public TabModel getCurrentModel() {
        return getModelAt(mActiveModelIndex);
    }

    @Override
    public int getCurrentModelIndex() {
        return mActiveModelIndex;
    }

    @Override
    public TabModel getModel(boolean incognito) {
        int index = incognito ? INCOGNITO_TAB_MODEL_INDEX : NORMAL_TAB_MODEL_INDEX;
        return getModelAt(index);
    }

    @Override
    public boolean isIncognitoSelected() {
        return mActiveModelIndex == INCOGNITO_TAB_MODEL_INDEX;
    }

    @Override
    public List<TabModel> getModels() {
        return mTabModels;
    }

    @Override
    public boolean closeTab(Tab tab) {
        for (int i = 0; i < getModels().size(); i++) {
            TabModel model = getModelAt(i);
            if (model.indexOf(tab) >= 0) {
                return model.closeTab(tab);
            }
        }
        assert false : "Tried to close a tab that is not in any model!";
        return false;
    }

    @Override
    public void commitAllTabClosures() {
        for (int i = 0; i < mTabModels.size(); i++) {
            mTabModels.get(i).commitAllTabClosures();
        }
    }

    @Override
    public Tab getTabById(int id) {
        for (int i = 0; i < getModels().size(); i++) {
            Tab tab = TabModelUtils.getTabById(getModelAt(i), id);
            if (tab != null) return tab;
        }
        return null;
    }

    @Override
    public void closeAllTabs() {
        for (int i = 0; i < getModels().size(); i++) getModelAt(i).closeAllTabs();
    }

    @Override
    public int getTotalTabCount() {
        int count = 0;
        for (int i = 0; i < getModels().size(); i++)  {
            count += getModelAt(i).getCount();
        }
        return count;
    }

    @Override
    public void registerChangeListener(ChangeListener changeListener) {
        if (!mChangeListeners.contains(changeListener)) mChangeListeners.add(changeListener);
    }

    @Override
    public void unregisterChangeListener(ChangeListener changeListener) {
        mChangeListeners.remove(changeListener);
    }

    /**
     * Notifies all the listeners that the {@link TabModelSelector} or its {@link TabModel} has
     * changed.
     */
    protected void notifyChanged() {
        for (int i = 0; i < mChangeListeners.size(); i++) {
            mChangeListeners.get(i).onChange();
        }
    }

    /**
     * Notifies all the listeners that a new tab has been created.
     * @param tab The tab that has been created.
     */
    protected void notifyNewTabCreated(Tab tab) {
        for (int i = 0; i < mChangeListeners.size(); i++) {
            mChangeListeners.get(i).onNewTabCreated(tab);
        }
    }
}
