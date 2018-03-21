// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modelutil;

import android.support.annotation.Nullable;

import org.chromium.base.ObserverList;

/**
 * A base class for models maintaining a list of items. Note that ListObservable models do not need
 * to be implemented as a list. Internally they may use any structure to organize their items.
 *
 * @param <E> The type of items held by the model.
 */
public abstract class ListObservable<E> {
    /**
     * An observer to be notified of changes to a {@link ListObservable}.
     *
     * @param <E> The type of items held by the observed model.
     */
    public interface ListObserver<E> {
        /**
         * Notifies that {@code count} items starting at position {@code index} under the
         * {@code source} have been added.
         *
         * @param source The list to which items have been added.
         * @param index The starting position of the range of added items.
         * @param count The number of added items.
         */
        void onItemRangeInserted(ListObservable<E> source, int index, int count);

        /**
         * Notifies that {@code count} items starting at position {@code index} under the
         * {@code source} have been removed.
         *
         * @param source The list from which items have been removed.
         * @param index The starting position of the range of removed items.
         * @param count The number of removed items.
         */
        void onItemRangeRemoved(ListObservable<E> source, int index, int count);

        /**
         * Notifies that {@code count} items starting at position {@code index} under the
         * {@code source} have changed, with an optional payload object.
         *
         * @param source The list whose items have changed.
         * @param index The starting position of the range of changed items.
         * @param count The number of changed items.
         * @param payload Optional parameter, use {@code null} to identify a "full" update.
         */
        void onItemRangeChanged(
                ListObservable<E> source, int index, int count, @Nullable Object payload);
    }

    private final ObserverList<ListObserver<E>> mObservers = new ObserverList<>();

    /** @return The total number of items held by the model. */
    public abstract int getItemCount();

    /**
     * Retrieve the item at the specified {@code position}.
     *
     * @param position The position of the item to retrieve.
     * @return The item at the specified {@code position}.
     */
    public abstract E getItem(int position);

    /**
     * @param observer An observer to be notified of changes to the model.
     */
    public void addObserver(ListObserver<E> observer) {
        mObservers.addObserver(observer);
    }

    /** @param observer The observer to remove. */
    public void removeObserver(ListObserver<E> observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Notifies observers that {@code count} items starting at position {@code index} have been
     * added.
     *
     * @param index The starting position of the range of added items.
     * @param count The number of added items.
     */
    protected void notifyItemRangeInserted(int index, int count) {
        for (ListObserver<E> observer : mObservers) {
            observer.onItemRangeInserted(this, index, count);
        }
    }

    /**
     * Notifies observes that {@code count} items starting at position {@code index} have been
     * removed.
     *
     * @param index The starting position of the range of removed items.
     * @param count The number of removed items.
     */
    protected void notifyItemRangeRemoved(int index, int count) {
        for (ListObserver<E> observer : mObservers) {
            observer.onItemRangeRemoved(this, index, count);
        }
    }

    /**
     * Notifies observers that {@code count} items starting at position {@code index} under the
     * {@code source} have changed, with an optional payload object.
     *
     * @param index The starting position of the range of changed items.
     * @param count The number of changed items.
     * @param payload Optional parameter, use {@code null} to identify a "full" update.
     */
    protected void notifyItemRangeChanged(int index, int count, @Nullable Object payload) {
        for (ListObserver<E> observer : mObservers) {
            observer.onItemRangeChanged(this, index, count, payload);
        }
    }
}
