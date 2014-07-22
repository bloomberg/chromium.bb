// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

/**
 * Base class for mojo generated interfaces that have a client.
 *
 * @param <C> the type of the client interface.
 */
public interface InterfaceWithClient<C extends Interface> extends Interface {

    /**
     * Set the client associated with this interface.
     */
    public void setClient(C client);

}
