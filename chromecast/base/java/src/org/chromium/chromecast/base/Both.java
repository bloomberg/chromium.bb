// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

/**
 * Represents a structure containing an instance of both A and B.
 *
 * In algebraic type theory, this is the Product type.
 *
 * This is useful for representing many objects in one without having to create a new class.
 * The type itself is composable, so the type parameters can themselves be Boths, and so on
 * recursively. This way, one can make a binary tree of types and wrap it in a single type that
 * represents the composition of all the leaf types.
 *
 * @param <A> The first type.
 * @param <B> The second type.
 */
public class Both<A, B> {
    public final A first;
    public final B second;

    private Both(A a, B b) {
        this.first = a;
        this.second = b;
    }

    // Can be used as a method reference.
    public A getFirst() {
        return this.first;
    }

    // Can be used as a method reference.
    public B getSecond() {
        return this.second;
    }

    @Override
    public String toString() {
        return new StringBuilder()
                .append(this.first.toString())
                .append(", ")
                .append(this.second.toString())
                .toString();
    }

    /**
     * Constructs a Both object containing both `a` and `b`.
     */
    public static <A, B> Both<A, B> both(A a, B b) {
        assert a != null;
        assert b != null;
        return new Both<>(a, b);
    }
}
