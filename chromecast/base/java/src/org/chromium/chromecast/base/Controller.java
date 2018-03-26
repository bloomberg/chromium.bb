// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.chromium.base.ObserverList;

import java.util.ArrayDeque;

/**
 * An Observable with public mutators that can control the state that it represents.
 *
 * The two mutators are set() and reset(). The set() method sets the state, and can inject
 * arbitrary data of the parameterized type, which will be forwarded to any observers of this
 * Controller. The reset() method deactivates the state.
 *
 * @param <T> The type of the state data.
 */
public class Controller<T> extends Observable<T> {
    private final ObserverList<ScopeFactory<? super T>> mEnterObservers = new ObserverList<>();
    private final ObserverList<Scope> mExitObservers = new ObserverList<>();
    private T mData = null;
    private boolean mIsNotifying = false;
    private final ArrayDeque<Runnable> mMessageQueue = new ArrayDeque<>();

    @Override
    protected void addObserver(ScopeFactory<? super T> observer) {
        mEnterObservers.addObserver(observer);
        if (mData != null) mExitObservers.addObserver(observer.create(mData));
    }

    /**
     * Activates all observers of this Controller.
     *
     * If this controller is already set(), an implicit reset() is called. This allows observing
     * scopes to properly clean themselves up before the scope for the new activation is
     * created.
     *
     * This can be called inside a scope that is triggered by this very controller. If set() is
     * called while handling another set() or reset() call on the same Controller, it will be
     * queued and handled synchronously after the current set() or reset() is resolved.
     */
    public void set(T data) {
        if (mIsNotifying) {
            mMessageQueue.add(() -> set(data));
            return;
        }
        // set(null) is equivalent to reset().
        if (data == null) {
            reset();
            return;
        }
        // If this Controller was already set(), call reset() so observing scopes can clean up
        // before they are notified of the new data.
        if (mData != null) {
            reset();
        }
        // Observers might call set() and reset() in their callbacks. These calls should be queued
        // rather than run synchronously to allow a well-defined order of events.
        mIsNotifying = true;
        mData = data;
        for (ScopeFactory<? super T> observer : mEnterObservers) {
            mExitObservers.addObserver(observer.create(data));
        }
        mIsNotifying = false;
        // Done notifying observers; process any queued set() or reset() calls that they posted.
        while (!mMessageQueue.isEmpty()) {
            mMessageQueue.removeFirst().run();
        }
    }

    /**
     * Deactivates all observers of this Controller.
     *
     * If this Controller is already reset(), this is a no-op.
     *
     * This can be called inside a scope that is triggered by this very controller. If reset()
     * is called while handling another set() or reset() call on the same Controller, it will be
     * queued and handled synchronously after the current set() or reset() is resolved.
     */
    public void reset() {
        if (mData == null) return;
        if (mIsNotifying) {
            mMessageQueue.add(() -> reset());
            return;
        }
        // Observers might call set() and reset() in their callbacks. These calls should be queued
        // rather than run synchronously to allow a well-defined order of events.
        mIsNotifying = true;
        mData = null;
        for (Scope observer : Itertools.reverse(mExitObservers)) {
            observer.close();
        }
        mExitObservers.clear();
        mIsNotifying = false;
        // Done notifying observers; process any queued set() or reset() calls that they posted.
        while (!mMessageQueue.isEmpty()) {
            mMessageQueue.removeFirst().run();
        }
    }
}
