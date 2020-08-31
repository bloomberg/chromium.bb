// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

/**
 * An {@link ObservableSupplier} that may be destroyed by anyone with a reference to the object.
 * This is useful if the class that constructs the object implementing this interface is not
 * responsible for its cleanup. For example, this may be useful when constructing an object
 * using the factory pattern.
 *
 * @param <E> The type of the wrapped object.
 */
public interface DestroyableObservableSupplier<E> extends ObservableSupplier<E> {
    /**
     * Destroy the supplier and the object it holds.
     */
    void destroy();
}
