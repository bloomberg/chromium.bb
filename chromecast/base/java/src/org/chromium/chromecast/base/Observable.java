// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.chromium.base.Log;

/**
 * Interface for Observable state.
 *
 * Observables can have some data associated with them, which is provided to observers when the
 * Observable activates. The <T> parameter determines the type of this data.
 *
 * Only this class has access to addObserver(). Clients should use the `watch()` method to track
 * the life cycle of Observables.
 *
 * @param <T> The type of the state data.
 */
public abstract class Observable<T> {
    private static final String TAG = "BaseObservable";

    protected abstract void addObserver(StateObserver<? super T> observer);

    /**
     * Tracks this Observable with the given scope factory.
     *
     * When this Observable is activated, the factory will be invoked with the activation data
     * to produce a scope. When this Observable is deactivated, that scope will have its close()
     * method invoked. In this way, one can define state transitions from the ScopeFactory and
     * its return value's close() method.
     */
    public final Observable<T> watch(ScopeFactory<? super T> factory) {
        addObserver(new StateObserver<>(factory));
        return this;
    }

    /**
     * Tracks this Observable with the given scope factory, ignoring activation data.
     *
     * A VoidScopeFactory does not care about the activation data, as its create() function
     * takes no arguments.
     */
    public final Observable<T> watch(VoidScopeFactory factory) {
        return watch((T value) -> factory.create());
    }

    /**
     * Creates an Observable that activates observers only if both `this` and `other` are activated,
     * and deactivates observers if either of `this` or `other` are deactivated.
     *
     * This is useful for creating an event handler that should only activate when two events
     * have occurred, but those events may occur in any order.
     *
     * This is composable (returns an Observable), so one can use this to observe the intersection
     * of arbitrarily many Observables.
     */
    public final <U> Observable<Both<T, U>> and(Observable<U> other) {
        return new BothStateObserver<>(this, other).asObservable();
    }

    /**
     * Returns an Observable that applies the given Function to this Observable's activation
     * values.
     *
     * @param <R> The return type of the transform function.
     */
    public final <R> Observable<R> transform(Function<? super T, ? extends R> transform) {
        Controller<R> controller = new Controller<>();
        watch((T value) -> {
            controller.set(transform.apply(value));
            return () -> controller.reset();
        });
        return controller;
    }

    /**
     * Returns an Observable that is activated only when the given Observable is not activated.
     */
    public static Observable<Unit> not(Observable<?> observable) {
        Controller<Unit> opposite = new Controller<>();
        opposite.set(Unit.unit());
        observable.watch(() -> {
            opposite.reset();
            return () -> opposite.set(Unit.unit());
        });
        return opposite;
    }

    // Adapter of ScopeFactory to two callbacks that Observables use to notify of state changes.
    protected static class StateObserver<T> {
        private final ScopeFactory<? super T> mFactory;
        private AutoCloseable mState;

        private StateObserver(ScopeFactory<? super T> factory) {
            super();
            mFactory = factory;
            mState = null;
        }

        protected final void onEnter(T data) {
            mState = mFactory.create(data);
        }

        protected final void onExit() {
            if (mState != null) {
                try {
                    mState.close();
                } catch (Exception e) {
                    Log.e(TAG, "Exception closing State scope", e);
                }
                mState = null;
            };
        }
    }

    // Owns a Controller that is activated only when both Observables are activated.
    private static class BothStateObserver<A, B> {
        private final Controller<Both<A, B>> mController;
        private A mA;
        private B mB;
        private Both<A, B> mBoth;

        private BothStateObserver(Observable<A> stateA, Observable<B> stateB) {
            mController = new Controller<>();
            mA = null;
            mB = null;
            mBoth = null;
            stateA.watch((A a) -> {
                if (mB != null) setBoth(a, mB);
                mA = a;
                return () -> {
                    mA = null;
                    resetBoth();
                };
            });
            stateB.watch((B b) -> {
                if (mA != null) setBoth(mA, b);
                mB = b;
                return () -> {
                    mB = null;
                    resetBoth();
                };
            });
        }

        private void setBoth(A a, B b) {
            mBoth = Both.both(a, b);
            mController.set(mBoth);
        }

        private void resetBoth() {
            mController.reset();
            mBoth = null;
        }

        private Observable<Both<A, B>> asObservable() {
            return mController;
        }
    }
}
