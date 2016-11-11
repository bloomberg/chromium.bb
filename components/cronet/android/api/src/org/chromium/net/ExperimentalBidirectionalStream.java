// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/**
 * {@link BidirectionalStream} that exposes experimental features.
 * Created using {@link Builder}. Every instance of {@link BidirectionalStream} can be casted
 * to an instance of this class.
 *
 * {@hide prototype}
 */
public abstract class ExperimentalBidirectionalStream extends BidirectionalStream {
    /**
     * Builder for {@link ExperimentalBidirectionalStream}s. Allows configuring stream before
     * constructing it via {@link Builder#build}. Created by
     * {@link ExperimentalCronetEngine#newBidirectionalStreamBuilder}. A reference to this class
     * can also be obtained through downcasting of {@link BidirectionalStream.Builder}.
     */
    public abstract static class Builder extends BidirectionalStream.Builder {
        /**
         * Associates the annotation object with this request. May add more than one.
         * Passed through to a {@link RequestFinishedInfo.Listener},
         * see {@link RequestFinishedInfo#getAnnotations}.
         *
         * @param annotation an object to pass on to the {@link RequestFinishedInfo.Listener} with a
         * {@link RequestFinishedInfo}.
         * @return the builder to facilitate chaining.
         */
        public Builder addRequestAnnotation(Object annotation) {
            return this;
        }

        // To support method chaining, override superclass methods to return an
        // instance of this class instead of the parent.

        @Override
        public abstract Builder setHttpMethod(String method);

        @Override
        public abstract Builder addHeader(String header, String value);

        @Override
        public abstract Builder setPriority(int priority);

        @Override
        public abstract Builder delayRequestHeadersUntilFirstFlush(
                boolean delayRequestHeadersUntilFirstFlush);

        @Override
        public abstract ExperimentalBidirectionalStream build();
    }
}
