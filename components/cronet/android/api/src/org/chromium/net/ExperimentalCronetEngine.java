// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net;

import android.content.Context;
import android.support.annotation.VisibleForTesting;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.util.Date;
import java.util.Set;
import java.util.concurrent.Executor;

/**
 * {@link CronetEngine} that exposes experimental features. Use {@link Builder} to build an
 * instance of this class. Every instance of {@link CronetEngine} can be casted to an instance
 * of this class.
 *
 * {@hide since this class exposes experimental features that should be hidden.}
 */
public abstract class ExperimentalCronetEngine extends CronetEngine {
    /**
     * The value of a connection metric is unknown.
     */
    public static final int CONNECTION_METRIC_UNKNOWN = -1;

    /**
     * The estimate of the effective connection type is unknown.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_UNKNOWN = 0;

    /**
     * The device is offline.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_OFFLINE = 1;

    /**
     * The estimate of the effective connection type is slow 2G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_SLOW_2G = 2;

    /**
     * The estimate of the effective connection type is 2G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_2G = 3;

    /**
     * The estimate of the effective connection type is 3G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_3G = 4;

    /**
     * The estimate of the effective connection type is 4G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_4G = 5;

    /**
     * Builder for building {@link ExperimentalCronetEngine}.
     */
    public static class Builder extends CronetEngine.Builder {
        /**
         * Constructs a {@link Builder} object that facilitates creating a
         * {@link CronetEngine}. The default configuration enables HTTP/2 and
         * disables QUIC, SDCH and the HTTP cache.
         *
         * @param context Android {@link Context}, which is used by
         *                {@link Builder} to retrieve the application
         *                context. A reference to only the application
         *                context will be kept, so as to avoid extending
         *                the lifetime of {@code context} unnecessarily.
         */
        public Builder(Context context) {
            super(context);
        }

        /**
         * Enables the network quality estimator, which collects and reports
         * measurements of round trip time (RTT) and downstream throughput at
         * various layers of the network stack. After enabling the estimator,
         * listeners of RTT and throughput can be added with
         * {@link #addRttListener} and {@link #addThroughputListener} and
         * removed with {@link #removeRttListener} and
         * {@link #removeThroughputListener}. The estimator uses memory and CPU
         * only when enabled.
         * @param value {@code true} to enable network quality estimator,
         *            {@code false} to disable.
         * @return the builder to facilitate chaining.
         */
        public Builder enableNetworkQualityEstimator(boolean value) {
            mBuilderDelegate.enableNetworkQualityEstimator(value);
            return this;
        }

        /**
         * Initializes CachingCertVerifier's cache with certVerifierData which has
         * the results of certificate verification.
         * @param certVerifierData a serialized representation of certificate
         *        verification results.
         * @return the builder to facilitate chaining.
         */
        public Builder setCertVerifierData(String certVerifierData) {
            mBuilderDelegate.setCertVerifierData(certVerifierData);
            return this;
        }

        /**
         * Overrides
         * <a href="https://developer.chrome.com/multidevice/data-compression">
         * Data Reduction Proxy</a> configuration parameters with a primary
         * proxy name, fallback proxy name, and a secure proxy check URL. Proxies
         * are specified as [scheme://]host[:port]. Used for testing.
         * @param primaryProxy the primary data reduction proxy to use.
         * @param fallbackProxy a fallback data reduction proxy to use.
         * @param secureProxyCheckUrl a URL to fetch to determine if using a secure
         * proxy is allowed.
         * @return the builder to facilitate chaining.
         */
        public Builder setDataReductionProxyOptions(
                String primaryProxy, String fallbackProxy, String secureProxyCheckUrl) {
            mBuilderDelegate.setDataReductionProxyOptions(
                    primaryProxy, fallbackProxy, secureProxyCheckUrl);
            return this;
        }

        /**
         * Sets whether the resulting {@link CronetEngine} uses an
         * implementation based on the system's
         * {@link java.net.HttpURLConnection} implementation, or if this is
         * only done as a backup if the native implementation fails to load.
         * Defaults to disabled.
         * @param value {@code true} makes the resulting {@link CronetEngine}
         *              use an implementation based on the system's
         *              {@link java.net.HttpURLConnection} implementation
         *              without trying to load the native implementation.
         *              {@code false} makes the resulting {@code CronetEngine}
         *              use the native implementation, or if that fails to load,
         *              falls back to an implementation based on the system's
         *              {@link java.net.HttpURLConnection} implementation.
         * @return the builder to facilitate chaining.
         */
        public Builder enableLegacyMode(boolean value) {
            mBuilderDelegate.enableLegacyMode(value);
            return this;
        }

        /**
         * Enables
         * <a href="https://developer.chrome.com/multidevice/data-compression">Data
         * Reduction Proxy</a>. Defaults to disabled.
         * @param key key to use when authenticating with the proxy.
         * @return the builder to facilitate chaining.
         */
        public Builder enableDataReductionProxy(String key) {
            mBuilderDelegate.enableDataReductionProxy(key);
            return this;
        }

        /**
         * Sets experimental options to be used in Cronet.
         *
         * @param options JSON formatted experimental options.
         * @return the builder to facilitate chaining.
         */
        public Builder setExperimentalOptions(String options) {
            mBuilderDelegate.setExperimentalOptions(options);
            return this;
        }

        @VisibleForTesting
        ICronetEngineBuilder getBuilderDelegate() {
            return mBuilderDelegate;
        }

        // To support method chaining, override superclass methods to return an
        // instance of this class instead of the parent.

        @Override
        public Builder setUserAgent(String userAgent) {
            super.setUserAgent(userAgent);
            return this;
        }

        @Override
        public Builder setStoragePath(String value) {
            super.setStoragePath(value);
            return this;
        }

        @Override
        public Builder setLibraryLoader(LibraryLoader loader) {
            super.setLibraryLoader(loader);
            return this;
        }

        @Override
        public Builder enableQuic(boolean value) {
            super.enableQuic(value);
            return this;
        }

        @Override
        public Builder enableHttp2(boolean value) {
            super.enableHttp2(value);
            return this;
        }

        @Override
        public Builder enableSdch(boolean value) {
            super.enableSdch(value);
            return this;
        }

        @Override
        public Builder enableHttpCache(int cacheMode, long maxSize) {
            super.enableHttpCache(cacheMode, maxSize);
            return this;
        }

        @Override
        public Builder addQuicHint(String host, int port, int alternatePort) {
            super.addQuicHint(host, port, alternatePort);
            return this;
        }

        @Override
        public Builder addPublicKeyPins(String hostName, Set<byte[]> pinsSha256,
                boolean includeSubdomains, Date expirationDate) {
            super.addPublicKeyPins(hostName, pinsSha256, includeSubdomains, expirationDate);
            return this;
        }

        @Override
        public Builder enablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
            super.enablePublicKeyPinningBypassForLocalTrustAnchors(value);
            return this;
        }

        @Override
        public ExperimentalCronetEngine build() {
            return mBuilderDelegate.build();
        }
    }

    /**
     * Creates a builder for {@link BidirectionalStream} objects. All callbacks for
     * generated {@code BidirectionalStream} objects will be invoked on
     * {@code executor}. {@code executor} must not run tasks on the
     * current thread, otherwise the networking operations may block and exceptions
     * may be thrown at shutdown time.
     *
     * @param url URL for the generated streams.
     * @param callback the {@link BidirectionalStream.Callback} object that gets invoked upon
     * different events occurring.
     * @param executor the {@link Executor} on which {@code callback} methods will be invoked.
     *
     * @return the created builder.
     */
    public abstract ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, BidirectionalStream.Callback callback, Executor executor);

    @Override
    public abstract ExperimentalUrlRequest.Builder newUrlRequestBuilder(
            String url, UrlRequest.Callback callback, Executor executor);

    /**
     * Starts NetLog logging to a specified directory with a bounded size. The NetLog will contain
     * events emitted by all live CronetEngines. The NetLog is useful for debugging.
     * The log can be viewed by stitching the files using net/log/stitch_net_log_files.py and
     * using a Chrome browser navigated to chrome://net-internals/#import
     * @param dirPath the directory where the log files will be created. It must already exist.
     *            NetLog files must not already exist in the directory. If actively logging,
     *            this method is ignored.
     * @param logAll {@code true} to include basic events, user cookies,
     *            credentials and all transferred bytes in the log. This option presents a
     *            privacy risk, since it exposes the user's credentials, and should only be
     *            used with the user's consent and in situations where the log won't be public.
     *            {@code false} to just include basic events.
     * @param maxSize the maximum total disk space in bytes that should be used by NetLog. Actual
     *            disk space usage may exceed this limit slightly.
     */
    public void startNetLogToDisk(String dirPath, boolean logAll, int maxSize) {}

    /**
     * Returns an estimate of the effective connection type computed by the network quality
     * estimator. Call {@link Builder#enableNetworkQualityEstimator} to begin computing this
     * value.
     *
     * @return the estimated connection type. The returned value is one of
     * {@link #EFFECTIVE_CONNECTION_TYPE_UNKNOWN EFFECTIVE_CONNECTION_TYPE_* }.
     */
    public int getEffectiveConnectionType() {
        return EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
    }

    /**
     * Configures the network quality estimator for testing. This must be called
     * before round trip time and throughput listeners are added, and after the
     * network quality estimator has been enabled.
     * @param useLocalHostRequests include requests to localhost in estimates.
     * @param useSmallerResponses include small responses in throughput
     * estimates.
     */
    public void configureNetworkQualityEstimatorForTesting(
            boolean useLocalHostRequests, boolean useSmallerResponses) {}

    /**
     * Registers a listener that gets called whenever the network quality
     * estimator witnesses a sample round trip time. This must be called
     * after {@link Builder#enableNetworkQualityEstimator}, and with throw an
     * exception otherwise. Round trip times may be recorded at various layers
     * of the network stack, including TCP, QUIC, and at the URL request layer.
     * The listener is called on the {@link java.util.concurrent.Executor} that
     * is passed to {@link Builder#enableNetworkQualityEstimator}.
     * @param listener the listener of round trip times.
     */
    public void addRttListener(NetworkQualityRttListener listener) {}

    /**
     * Removes a listener of round trip times if previously registered with
     * {@link #addRttListener}. This should be called after a
     * {@link NetworkQualityRttListener} is added in order to stop receiving
     * observations.
     * @param listener the listener of round trip times.
     */
    public void removeRttListener(NetworkQualityRttListener listener) {}

    /**
     * Registers a listener that gets called whenever the network quality
     * estimator witnesses a sample throughput measurement. This must be called
     * after {@link Builder#enableNetworkQualityEstimator}. Throughput observations
     * are computed by measuring bytes read over the active network interface
     * at times when at least one URL response is being received. The listener
     * is called on the {@link java.util.concurrent.Executor} that is passed to
     * {@link Builder#enableNetworkQualityEstimator}.
     * @param listener the listener of throughput.
     */
    public void addThroughputListener(NetworkQualityThroughputListener listener) {}

    /**
     * Removes a listener of throughput. This should be called after a
     * {@link NetworkQualityThroughputListener} is added with
     * {@link #addThroughputListener} in order to stop receiving observations.
     * @param listener the listener of throughput.
     */
    public void removeThroughputListener(NetworkQualityThroughputListener listener) {}

    /**
     * Establishes a new connection to the resource specified by the {@link URL} {@code url}
     * using the given proxy.
     * <p>
     * <b>Note:</b> Cronet's {@link java.net.HttpURLConnection} implementation is subject to certain
     * limitations, see {@link #createURLStreamHandlerFactory} for details.
     *
     * @param url URL of resource to connect to.
     * @param proxy proxy to use when establishing connection.
     * @return an {@link java.net.HttpURLConnection} instance implemented by this CronetEngine.
     * @throws IOException if an error occurs while opening the connection.
     */
    // TODO(pauljensen): Expose once implemented, http://crbug.com/418111
    public URLConnection openConnection(URL url, Proxy proxy) throws IOException {
        return url.openConnection(proxy);
    }

    /**
     * Registers a listener that gets called after the end of each request with the request info.
     *
     * <p>The listener is called on an {@link java.util.concurrent.Executor} provided by the
     * listener.
     *
     * @param listener the listener for finished requests.
     */
    public void addRequestFinishedListener(RequestFinishedInfo.Listener listener) {}

    /**
     * Removes a finished request listener.
     *
     * @param listener the listener to remove.
     */
    public void removeRequestFinishedListener(RequestFinishedInfo.Listener listener) {}

    /**
     * Returns serialized representation of certificate verifier's cache
     * which contains the list of hosts/certificates and the certificate
     * verification results. May block until data is received from the network
     * thread (will timeout after the specified timeout). In case of timeout, it
     * returns the previous saved value.
     *
     * @param timeout in milliseconds. If timeout is 0, it will use default value.
     * @return serialized representation of certificate verification results
     *         data.
     */
    public String getCertVerifierData(long timeout) {
        return "";
    }

    /**
     * Returns the HTTP RTT estimate (in milliseconds) computed by the network
     * quality estimator. Set to {@link #CONNECTION_METRIC_UNKNOWN} if the value
     * is unavailable. This must be called after
     * {@link Builder#enableNetworkQualityEstimator}, and will throw an
     * exception otherwise.
     * @return Estimate of the HTTP RTT in milliseconds.
     */
    public int getHttpRttMs() {
        return CONNECTION_METRIC_UNKNOWN;
    }

    /**
     * Returns the transport RTT estimate (in milliseconds) computed by the
     * network quality estimator. Set to {@link #CONNECTION_METRIC_UNKNOWN} if
     * the value is unavailable. This must be called after
     * {@link Builder#enableNetworkQualityEstimator}, and will throw an
     * exception otherwise.
     * @return Estimate of the transport RTT in milliseconds.
     */
    public int getTransportRttMs() {
        return CONNECTION_METRIC_UNKNOWN;
    }

    /**
     * Returns the downstream throughput estimate (in kilobits per second)
     * computed by the network quality estimator. Set to
     * {@link #CONNECTION_METRIC_UNKNOWN} if the value is
     * unavailable. This must be called after
     * {@link Builder#enableNetworkQualityEstimator}, and will
     * throw an exception otherwise.
     * @return Estimate of the downstream throughput in kilobits per second.
     */
    public int getDownstreamThroughputKbps() {
        return CONNECTION_METRIC_UNKNOWN;
    }
}
