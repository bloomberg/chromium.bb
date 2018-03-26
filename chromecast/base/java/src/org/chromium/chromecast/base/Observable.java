// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

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
    protected abstract void addObserver(ScopeFactory<? super T> observer);

    /**
     * Tracks this Observable with the given scope factory.
     *
     * When this Observable is activated, the factory will be invoked with the activation data
     * to produce a scope. When this Observable is deactivated, that scope will have its close()
     * method invoked. In this way, one can define state transitions from the ScopeFactory and
     * its return value's close() method.
     */
    public final Observable<T> watch(ScopeFactory<? super T> factory) {
        addObserver(factory);
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
     * Returns an Observable that is activated when `this` and `other` are activated in order.
     *
     * This is similar to `and()`, but does not activate if `other` is activated before `this`.
     *
     * @param <U> The activation data type of the other Observable.
     */
    public final <U> Observable<Both<T, U>> andThen(Observable<U> other) {
        return new SequenceStateObserver<>(this, other).asObservable();
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

    // Owns a Controller that is activated only when both Observables are activated.
    private static class BothStateObserver<A, B> {
        private final Controller<Both<A, B>> mController = new Controller<>();
        private A mA = null;
        private B mB = null;

        private BothStateObserver(Observable<A> stateA, Observable<B> stateB) {
            stateA.watch((A a) -> {
                if (mB != null) mController.set(Both.both(a, mB));
                mA = a;
                return () -> {
                    mA = null;
                    mController.reset();
                };
            });
            stateB.watch((B b) -> {
                if (mA != null) mController.set(Both.both(mA, b));
                mB = b;
                return () -> {
                    mB = null;
                    mController.reset();
                };
            });
        }

        private Observable<Both<A, B>> asObservable() {
            return mController;
        }
    }

    // Owns a Controller that is activated only when the Observables are activated in order.
    private static class SequenceStateObserver<A, B> {
        private final Controller<Both<A, B>> mController = new Controller<>();
        private A mA = null;

        private SequenceStateObserver(Observable<A> stateA, Observable<B> stateB) {
            stateA.watch((A a) -> {
                mA = a;
                return () -> {
                    mA = null;
                    mController.reset();
                };
            });
            stateB.watch((B b) -> {
                if (mA != null) {
                    mController.set(Both.both(mA, b));
                }
                return () -> {
                    mController.reset();
                };
            });
        }

        private Observable<Both<A, B>> asObservable() {
            return mController;
        }
    }
}
