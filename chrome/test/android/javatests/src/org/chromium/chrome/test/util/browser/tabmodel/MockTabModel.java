// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel;

import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;

/**
 * Almost empty implementation to mock a TabModel. It only handles tab creation and queries.
 */
public class MockTabModel extends EmptyTabModel {
    private int mIndex = TabModel.INVALID_TAB_INDEX;

    private final ArrayList<Tab> mTabs = new ArrayList<Tab>();
    private final boolean mIncognito;

    public MockTabModel(boolean incognito) {
        mIncognito = incognito;
    }

    public void addTab(int id) {
        mTabs.add(new Tab(id, isIncognito(), null, null));
    }

    @Override
    public boolean isIncognito() {
        return mIncognito;
    }

    @Override
    public int getCount() {
        return mTabs.size();
    }

    @Override
    public Tab getTabAt(int position) {
        return mTabs.get(position);
    }

    @Override
    public int indexOf(Tab tab) {
        return mTabs.indexOf(tab);
    }

    @Override
    public int index() {
        return mIndex;
    }

    @Override
    public void setIndex(int i, TabSelectionType type) {
        mIndex = i;
    }
}