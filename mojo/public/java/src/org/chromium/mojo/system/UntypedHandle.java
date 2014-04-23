// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;


/**
 * TODO(qsr): Insert description here.
 */
public interface UntypedHandle extends Handle {

    /**
     * TODO(qsr):
     *
     * @return TODO(qsr)
     */
    public MessagePipeHandle toMessagePipeHandle();

    /**
     * TODO(qsr):
     *
     * @return TODO(qsr)
     */
    public DataPipe.ConsumerHandle toDataPipeConsumerHandle();

    /**
     * TODO(qsr):
     *
     * @return TODO(qsr)
     */
    public DataPipe.ProducerHandle toDataPipeProducerHandle();

    /**
     * TODO(qsr):
     *
     * @return TODO(qsr)
     */
    public SharedBufferHandle toSharedBufferHandle();

}
