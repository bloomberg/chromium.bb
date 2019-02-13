// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.support.annotation.NonNull;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * This class is responsible for filtering tabs from {@link TabModel}. The filtering logic is
 * delegated to the concrete class that extends this abstract class. If no filter is active, this
 * class has the same {@link TabList} as {@link TabModel} does.
 *
 * If there is at least one filter active, this is a {@link TabList} that contains the most
 * important tabs that the filter defines.
 */
public abstract class TabModelFilter extends EmptyTabModelObserver implements TabList {
    private static final List<Tab> sEmptyRelatedTabList =
            Collections.unmodifiableList(new ArrayList<Tab>());
    private TabModel mTabModel;
    protected ObserverList<TabModelFilterObserver> mFilteredObservers = new ObserverList<>();

    public TabModelFilter(TabModel tabModel) {
        mTabModel = tabModel;
        mTabModel.addObserver(this);
    }

    /**
     * Adds a {@link TabModelFilterObserver} to be notified on {@link TabModelFilter} changes.
     * @param observer The {@link TabModelFilterObserver} to add.
     */
    public void addObserver(TabModelFilterObserver observer) {
        mFilteredObservers.addObserver(observer);
    }

    /**
     * Removes a {@link TabModelFilterObserver}.
     * @param observer The {@link TabModelFilterObserver} to remove.
     */
    public void removeObserver(TabModelFilterObserver observer) {
        mFilteredObservers.removeObserver(observer);
    }

    /**
     * @return The {@link TabModel} that the filter is acting on.
     */
    protected TabModel getTabModel() {
        return mTabModel;
    }

    /**
     * Any of the concrete class can override and define a relationship that links a {@link Tab} to
     * a list of related {@link Tab}s. By default, this returns an empty unmodifiable list. Note
     * that the meaning of related can vary depending on the filter being applied.
     * @param tabId Id of the {@link Tab} try to relate with.
     * @return An unmodifiable list of {@link Tab} that relate with the given tab id.
     */
    @NonNull
    public List<Tab> getUnmodifiableRelatedTabList(int tabId) {
        return sEmptyRelatedTabList;
    }

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} added a
     * {@link Tab}.
     * @param tab {@link Tab} had added to {@link TabModel}.
     */
    protected abstract void addTab(Tab tab);

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} closed a
     * {@link Tab}.
     * @param tab {@link Tab} had closed from {@link TabModel}.
     */
    protected abstract void closeTab(Tab tab);

    /**
     * Concrete class requires to define what's the behavior when {@link TabModel} selected a
     * {@link Tab}.
     * @param tab {@link Tab} had selected.
     */
    protected abstract void selectTab(Tab tab);

    // TabModelObserver implementation
    // TODO(meiliang): All these implementations should notify each individual listener, after they
    // had added to TabModelFilterObserver.
    @Override
    public void didSelectTab(Tab tab, int type, int lastId) {
        selectTab(tab);
        for (TabModelFilterObserver observer : mFilteredObservers) {
            observer.update();
        }
    }

    @Override
    public void willCloseTab(Tab tab, boolean animate) {
        closeTab(tab);
        for (TabModelFilterObserver observer : mFilteredObservers) {
            observer.update();
        }
    }

    @Override
    public void didAddTab(Tab tab, int type) {
        addTab(tab);
        for (TabModelFilterObserver observer : mFilteredObservers) {
            observer.update();
        }
    }

    @Override
    public void tabClosureUndone(Tab tab) {
        addTab(tab);
        for (TabModelFilterObserver observer : mFilteredObservers) {
            observer.update();
        }
    }
}
