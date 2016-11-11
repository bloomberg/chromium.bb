// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net;

import java.util.concurrent.Executor;

/**
 * {@link UrlRequest} that exposes experimental features. Created using
 * {@link ExperimentalUrlRequest.Builder}. Every instance of {@link UrlRequest} can
 * be casted to an instance of this class.
 *
 * {@hide since this class exposes experimental features that should be hidden}.
 */
public abstract class ExperimentalUrlRequest extends UrlRequest {
    /**
     * Builder for building {@link UrlRequest}. Created by
     * {@link ExperimentalCronetEngine#newUrlRequestBuilder}. A reference to this class
     * can also be obtained through downcasting of {@link UrlRequest.Builder}.
     */
    public abstract static class Builder extends UrlRequest.Builder {
        /**
         * Disables connection migration for the request if enabled for
         * the session.
         * @return the builder to facilitate chaining.
         */
        public Builder disableConnectionMigration() {
            return this;
        }

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
        public abstract Builder disableCache();

        @Override
        public abstract Builder setPriority(int priority);

        @Override
        public abstract Builder setUploadDataProvider(
                UploadDataProvider uploadDataProvider, Executor executor);

        @Override
        public abstract Builder allowDirectExecutor();

        @Override
        public abstract ExperimentalUrlRequest build();
    }
}
