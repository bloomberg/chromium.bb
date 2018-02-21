// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * Helper functions for creating ScopeFactories, used by Observable.watch() to handle state changes.
 */
public final class ScopeFactories {
    // Uninstantiable.
    private ScopeFactories() {}

    /**
     * Shorthand for making a ScopeFactory that only has side effects on activation.
     *
     * @param <T> The type of the activation data.
     */
    public static <T> ScopeFactory<T> onEnter(Consumer<T> consumer) {
        return (T value) -> {
            consumer.accept(value);
            return () -> {};
        };
    }

    /**
     * Shorthand for making a ScopeFactory that only has side effects on activation, and is
     * independent of the activation value.
     *
     * @param <T> The type of the activation data.
     */
    public static <T> ScopeFactory<T> onEnter(Runnable runnable) {
        return onEnter((T value) -> runnable.run());
    }

    /**
     * Shorthand for making a ScopeFactory that only has side effects on deactivation.
     *
     * @param <T> The type of the activation data.
     */
    public static <T> ScopeFactory<T> onExit(Consumer<T> consumer) {
        return (T value) -> () -> consumer.accept(value);
    }

    /**
     * Shorthand for making a ScopeFactory that only has side effects on deactivation, and is
     * independent of the activation data.
     *
     * @param <T> The type of the activation data.
     */
    public static <T> ScopeFactory<T> onExit(Runnable runnable) {
        return onExit((T value) -> runnable.run());
    }
}
