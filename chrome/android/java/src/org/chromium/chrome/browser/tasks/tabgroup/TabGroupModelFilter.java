// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tabgroup;

import android.support.annotation.NonNull;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * An implementation of {@link TabModelFilter} that puts {@link Tab}s into a group
 * structure.
 *
 * A group is a collection of {@link Tab}s that share a common ancestor {@link Tab}. This filter is
 * also a {@link TabList} that contains the last shown {@link Tab} from every group.
 */
public class TabGroupModelFilter extends TabModelFilter {
    /**
     * This class is a representation of a group of tabs. It knows the last selected tab within the
     * group.
     */
    private class TabGroup {
        private final static int INVALID_GROUP_ID = -1;
        private final Set<Integer> mTabIds;
        private int mLastShownTabId;
        private int mGroupId;

        TabGroup(int groupId) {
            mTabIds = new LinkedHashSet<>();
            mLastShownTabId = Tab.INVALID_TAB_ID;
            mGroupId = groupId;
        }

        void addTab(int tabId) {
            mTabIds.add(tabId);
            if (mLastShownTabId == Tab.INVALID_TAB_ID) setLastShownTabId(tabId);
            if (size() > 1) reorderGroup(mGroupId);
        }

        void removeTab(int tabId) {
            assert mTabIds.contains(tabId);
            if (mLastShownTabId == tabId) {
                int nextIdToShow = nextTabIdToShow(tabId);
                if (nextIdToShow != Tab.INVALID_TAB_ID) setLastShownTabId(nextIdToShow);
            }
            mTabIds.remove(tabId);
        }

        void moveToEndInGroup(int tabId) {
            if (!mTabIds.contains(tabId)) return;
            mTabIds.remove(tabId);
            mTabIds.add(tabId);
        }

        boolean contains(int tabId) {
            return mTabIds.contains(tabId);
        }

        int size() {
            return mTabIds.size();
        }

        List<Integer> getTabIdList() {
            return Collections.unmodifiableList(new ArrayList<>(mTabIds));
        }

        int getLastShownTabId() {
            return mLastShownTabId;
        }

        void setLastShownTabId(int tabId) {
            assert mTabIds.contains(tabId);
            mLastShownTabId = tabId;
        }

        int nextTabIdToShow(int tabId) {
            if (mTabIds.size() == 1 || !mTabIds.contains(tabId)) return Tab.INVALID_TAB_ID;
            List<Integer> ids = getTabIdList();
            int position = ids.indexOf(tabId);
            if (position == 0) return ids.get(position + 1);
            return ids.get(position - 1);
        }
    }
    private Map<Integer, Integer> mGroupIdToGroupIndexMap = new HashMap<>();
    private Map<Integer, TabGroup> mGroupIdToGroupMap = new HashMap<>();
    private int mCurrentGroupIndex = TabList.INVALID_TAB_INDEX;

    public TabGroupModelFilter(TabModel tabModel) {
        super(tabModel);
    }

    // TabModelFilter implementation.
    @NonNull
    @Override
    public List<Tab> getRelatedTabList(int id) {
        // TODO(meiliang): In worst case, this method runs in O(n^2). This method needs to perform
        // better, especially when we try to call it in a loop for all tabs.
        Tab tab = TabModelUtils.getTabById(getTabModel(), id);
        if (tab == null) return super.getRelatedTabList(id);

        int groupId = tab.getRootId();
        TabGroup group = mGroupIdToGroupMap.get(groupId);
        return getRelatedTabList(group.getTabIdList());
    }

    private List<Tab> getRelatedTabList(List<Integer> ids) {
        List<Tab> tabs = new ArrayList<>();
        for (Integer id : ids) {
            tabs.add(TabModelUtils.getTabById(getTabModel(), id));
        }
        return Collections.unmodifiableList(tabs);
    }

    @Override
    protected void addTab(Tab tab) {
        if (tab.isIncognito() != isIncognito()) {
            throw new IllegalStateException("Attempting to open tab in the wrong model");
        }

        int groupId = tab.getRootId();
        if (mGroupIdToGroupMap.containsKey(groupId)) {
            mGroupIdToGroupMap.get(groupId).addTab(tab.getId());
        } else {
            TabGroup tabGroup = new TabGroup(tab.getRootId());
            tabGroup.addTab(tab.getId());
            mGroupIdToGroupMap.put(groupId, tabGroup);
            mGroupIdToGroupIndexMap.put(groupId, mGroupIdToGroupIndexMap.size());
        }
    }

    @Override
    protected void closeTab(Tab tab) {
        int groupId = tab.getRootId();
        if (tab.isIncognito() != isIncognito() || mGroupIdToGroupMap.get(groupId) == null
                || !mGroupIdToGroupMap.get(groupId).contains(tab.getId())) {
            throw new IllegalStateException("Attempting to close tab in the wrong model");
        }

        TabGroup group = mGroupIdToGroupMap.get(groupId);
        group.removeTab(tab.getId());
        if (group.size() == 0) {
            updateGroupIdToGroupIndexMapAfterGroupClosed(groupId);
            mGroupIdToGroupIndexMap.remove(groupId);
            mGroupIdToGroupMap.remove(groupId);
        }
    }

    private void updateGroupIdToGroupIndexMapAfterGroupClosed(int groupId) {
        int indexToRemove = mGroupIdToGroupIndexMap.get(groupId);
        Set<Integer> groupIdSet = mGroupIdToGroupIndexMap.keySet();
        for (Integer groupIdKey : groupIdSet) {
            int groupIndex = mGroupIdToGroupIndexMap.get(groupIdKey);
            if (groupIndex > indexToRemove) {
                mGroupIdToGroupIndexMap.put(groupIdKey, groupIndex - 1);
            }
        }
    }

    @Override
    protected void selectTab(Tab tab) {
        int groupId = tab.getRootId();
        mGroupIdToGroupMap.get(groupId).setLastShownTabId(tab.getId());
        mCurrentGroupIndex = mGroupIdToGroupIndexMap.get(groupId);
    }

    @Override
    protected void reorder() {
        reorderGroup(TabGroup.INVALID_GROUP_ID);

        TabModel tabModel = getTabModel();
        selectTab(tabModel.getTabAt(tabModel.index()));

        assert mGroupIdToGroupIndexMap.size() == mGroupIdToGroupMap.size();
    }

    private void reorderGroup(int groupId) {
        boolean reorderAllGroups = groupId == TabGroup.INVALID_GROUP_ID;
        if (reorderAllGroups) {
            mGroupIdToGroupIndexMap.clear();
        }

        TabModel tabModel = getTabModel();
        for (int i = 0; i < tabModel.getCount(); i++) {
            Tab tab = tabModel.getTabAt(i);
            if (reorderAllGroups) {
                groupId = tab.getRootId();
                if (!mGroupIdToGroupIndexMap.containsKey(groupId)) {
                    mGroupIdToGroupIndexMap.put(groupId, mGroupIdToGroupIndexMap.size());
                }
            }
            mGroupIdToGroupMap.get(groupId).moveToEndInGroup(tab.getId());
        }
    }

    // TabList implementation.
    @Override
    public boolean isIncognito() {
        return getTabModel().isIncognito();
    }

    @Override
    public int index() {
        return mCurrentGroupIndex;
    }

    @Override
    public int getCount() {
        return mGroupIdToGroupMap.size();
    }

    @Override
    public Tab getTabAt(int index) {
        if (index < 0 || index >= getCount()) return null;
        int groupId = Tab.INVALID_TAB_ID;
        Set<Integer> groupIdSet = mGroupIdToGroupIndexMap.keySet();
        for (Integer groupIdKey : groupIdSet) {
            if (mGroupIdToGroupIndexMap.get(groupIdKey) == index) {
                groupId = groupIdKey;
                break;
            }
        }
        if (groupId == Tab.INVALID_TAB_ID) return null;

        return TabModelUtils.getTabById(
                getTabModel(), mGroupIdToGroupMap.get(groupId).getLastShownTabId());
    }

    @Override
    public int indexOf(Tab tab) {
        if (tab == null) return TabList.INVALID_TAB_INDEX;
        return mGroupIdToGroupIndexMap.get(tab.getId());
    }

    @Override
    public boolean isClosurePending(int tabId) {
        return getTabModel().isClosurePending(tabId);
    }
}
