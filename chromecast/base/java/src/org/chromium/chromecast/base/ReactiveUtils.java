// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * Helper functions for working with Observables.
 */
public final class ReactiveUtils {
    // Uninstantiable.
    private ReactiveUtils() {}

    /**
     * Interface representing actions to perform when activating and deactivating a state whose type
     * is a Both structure. The arguments to the create() function are the fields of the Both value.
     *
     * @param <A> The type of the first argument to create().
     * @param <B> The type of the second argument to create().
     */
    public interface BothScopeFactory<A, B> { public AutoCloseable create(A first, B second); }

    /**
     * A helper to make reacting to combinations of two Observables more readable.
     *
     * This disassembles the Both value observed by the `.and()`-conjunction of Observables and
     * injects the constituent fields in the BothScopeFactory's create() function.
     *
     * This allows you to rewrite this:
     *
     *     observableA.and(observableB).watch((Both<A, B> data) -> {
     *         A a = data.first;
     *         B b = data.second;
     *         ...
     *     });
     *
     * as this:
     *
     *     watchBoth(observableA.and(observableB), (A a, B b) -> ...);
     *
     * This is not as composable as chained `.and()` calls, which can be extended to an arbitrary
     * number of parameters. But it is more readable for the case where you are observing two
     * Observables.
     *
     * @param <A> The first type in the Both structure.
     * @param <B> The second type in the Both structure.
     */
    public static <A, B> Observable<Both<A, B>> watchBoth(Observable<Both<A, B>> observable,
            BothScopeFactory<? super A, ? super B> scopeFactory) {
        return observable.watch((Both<A, B> data) -> scopeFactory.create(data.first, data.second));
    }
}
