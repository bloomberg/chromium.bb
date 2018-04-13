// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyObservable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * As model of the keyboard accessory component, this class holds the data relevant to the visual
 * state of the accessory.
 * This includes the visibility of the accessory in general, any available tabs and actions.
 * Whenever the state changes, it notifies its listeners - like the
 * {@link KeyboardAccessoryMediator} or the ModelChangeProcessor.
 */
class KeyboardAccessoryModel extends PropertyObservable<KeyboardAccessoryModel.PropertyKey> {
    /** Keys uniquely identifying model properties. */
    static class PropertyKey {
        static final PropertyKey VISIBLE = new PropertyKey();
        private PropertyKey() {}
    }

    /** A {@link ListObservable} containing an {@link ArrayList} of Tabs or Actions. */
    class SimpleListObservable<T> extends ListObservable {
        private final List<T> mItems = new ArrayList<>();

        public T get(int index) {
            return mItems.get(index);
        }

        @Override
        public int getItemCount() {
            return mItems.size();
        }

        void add(T item) {
            mItems.add(item);
            notifyItemRangeInserted(mItems.size() - 1, 1);
        }

        void remove(T item) {
            int position = mItems.indexOf(item);
            if (position == -1) {
                return;
            }
            mItems.remove(position);
            notifyItemRangeRemoved(position, 1);
        }

        void set(T[] newItems) {
            if (mItems.isEmpty()) {
                if (newItems.length == 0) {
                    return; // Nothing to do, nothing changes.
                }
                mItems.addAll(Arrays.asList(newItems));
                notifyItemRangeInserted(0, mItems.size());
                return;
            }
            int oldSize = mItems.size();
            mItems.clear();
            if (newItems.length == 0) {
                notifyItemRangeRemoved(0, oldSize);
                return;
            }
            mItems.addAll(Arrays.asList(newItems));
            notifyItemRangeChanged(0, Math.max(oldSize, mItems.size()), this);
        }
    }

    private SimpleListObservable<KeyboardAccessoryData.Action> mActionListObservable;
    private SimpleListObservable<KeyboardAccessoryData.Tab> mTabListObservable;
    private boolean mVisible;

    KeyboardAccessoryModel() {
        mActionListObservable = new SimpleListObservable<>();
        mTabListObservable = new SimpleListObservable<>();
    }

    void addActionListObserver(ListObservable.ListObserver observer) {
        mActionListObservable.addObserver(observer);
    }

    void setActions(KeyboardAccessoryData.Action[] actions) {
        mActionListObservable.set(actions);
    }

    SimpleListObservable<KeyboardAccessoryData.Action> getActionList() {
        return mActionListObservable;
    }

    void addTabListObserver(ListObservable.ListObserver observer) {
        mTabListObservable.addObserver(observer);
    }

    void addTab(KeyboardAccessoryData.Tab tab) {
        mTabListObservable.add(tab);
    }

    void removeTab(KeyboardAccessoryData.Tab tab) {
        mTabListObservable.remove(tab);
    }

    SimpleListObservable<KeyboardAccessoryData.Tab> getTabList() {
        return mTabListObservable;
    }

    void setVisible(boolean visible) {
        mVisible = visible;
        notifyPropertyChanged(PropertyKey.VISIBLE);
    }

    boolean isVisible() {
        return mVisible;
    }
}
