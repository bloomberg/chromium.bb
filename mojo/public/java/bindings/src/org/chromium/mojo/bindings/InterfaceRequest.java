// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

/**
 * Used in methods that return instances of remote objects. Allows to start using the remote object
 * immediately, while sending the handle that will be bind to the implementation.
 *
 * @param <I> the type of the remote interface.
 */
public final class InterfaceRequest<I extends Interface> {

}
