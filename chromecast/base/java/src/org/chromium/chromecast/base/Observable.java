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
    /**
     * Tracks this Observable with the given scope factory.
     *
     * Returns a Scope that, when closed, will unregister the scope factory so that it will no
     * longer be notified of updates.
     *
     * When this Observable is activated, the factory will be invoked with the activation data
     * to produce a scope. When this Observable is deactivated, that scope will have its close()
     * method invoked. In this way, one can define state transitions from the ScopeFactory and
     * its return value's close() method.
     */
    public abstract Scope watch(ScopeFactory<? super T> factory);

    /**
     * Tracks this Observable with the given scope factory, ignoring activation data.
     *
     * Returns a Scope that, when closed, will unregister the scope factory so that it will no
     * longer be notifies of updates.
     *
     * A VoidScopeFactory does not care about the activation data, as its create() function
     * takes no arguments.
     */
    public final Scope watch(VoidScopeFactory factory) {
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
        Controller<Both<T, U>> controller = new Controller<>();
        watch(t -> other.watch(u -> {
            controller.set(Both.both(t, u));
            return controller::reset;
        }));
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
        Controller<U> otherAfterThis = new Controller<>();
        other.watch((U value) -> {
            otherAfterThis.set(value);
            return otherAfterThis::reset;
        });
        watch(ScopeFactories.onEnter(x -> otherAfterThis.reset()));
        return and(otherAfterThis);
    }

    /**
     * Returns an Observable that applies the given Function to this Observable's activation
     * values.
     *
     * @param <R> The return type of the transform function.
     */
    public final <R> Observable<R> map(Function<? super T, ? extends R> transform) {
        Controller<R> controller = new Controller<>();
        watch((T value) -> {
            controller.set(transform.apply(value));
            return controller::reset;
        });
        return controller;
    }

    /**
     * Returns an Observable that is only activated when `this` is activated with a value such that
     * the given `predicate` returns true.
     */
    public final Observable<T> filter(Predicate<? super T> predicate) {
        Controller<T> controller = new Controller<>();
        watch((T value) -> {
            if (predicate.test(value)) {
                controller.set(value);
            }
            return controller::reset;
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
}
