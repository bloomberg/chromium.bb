// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Different values of the effective connection type as computed by the network
 * quality estimator. EffectiveConnectionType is the connection type whose
 * typical performance is most similar to the measured performance of the
 * network in use. In many cases, the "effective" connection type and the actual
 * type of connection in use are the same, but often a network connection
 * performs significantly differently, usually worse, from its expected
 * capabilities. EffectiveConnectionType of a network is independent of if the
 * current connection is metered or not. For example, an unmetered slow
 * connection may have EFFECTIVE_CONNECTION_TYPE_SLOW_2G as its effective
 * connection type.
 * {@hide} as it's a prototype.
 */
public class EffectiveConnectionType {
    /** {@hide} */
    @IntDef({
            EFFECTIVE_CONNECTION_TYPE_UNKNOWN, EFFECTIVE_CONNECTION_TYPE_OFFLINE,
            EFFECTIVE_CONNECTION_TYPE_SLOW_2G, EFFECTIVE_CONNECTION_TYPE_2G,
            EFFECTIVE_CONNECTION_TYPE_3G, EFFECTIVE_CONNECTION_TYPE_4G,
            EFFECTIVE_CONNECTION_TYPE_BROADBAND,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface EffectiveConnectionTypeValues {}

    /**
     * Effective connection type reported when the network quality is unknown.
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_UNKNOWN = 0;

    /**
     * Effective connection type reported when the Internet is unreachable,
     * either because the device does not have a connection or because the
     * connection is too slow to be usable.
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_OFFLINE = 1;

    /**
     * Effective connection type reported when the network has the quality of a
     * poor 2G connection.
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_SLOW_2G = 2;

    /**
     * Effective connection type reported when the network has the quality of a
     * faster 2G connection.
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_2G = 3;

    /**
     * Effective connection type reported when the network has the quality of a
     * 3G connection.
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_3G = 4;

    /**
     * Effective connection type reported when the network has the quality of a
     * 4G connection.
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_4G = 5;

    /**
     * Effective connection type reported when the network has the quality of a
     * broadband connection.
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_BROADBAND = 6;

    private EffectiveConnectionType() {}

    /**
     * Maps the effective connection type enum computed by the network quality
     * estimator to {@link EffectiveConnectionType}.
     * @param effectiveConnectionType Effective connection type enum computed by
     *            the network quality estimator.
     * @return {@link EffectiveConnectionType} corresponding to the provided enum.
     * @hide only used by internal implementation.
     */
    @EffectiveConnectionTypeValues
    public static int getEffectiveConnectionType(int effectiveConnectionType) {
        assert ChromiumEffectiveConnectionType.EFFECTIVE_CONNECTION_TYPE_LAST == 7;
        switch (effectiveConnectionType) {
            case ChromiumEffectiveConnectionType.EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
                return EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
            case ChromiumEffectiveConnectionType.EFFECTIVE_CONNECTION_TYPE_OFFLINE:
                return EFFECTIVE_CONNECTION_TYPE_OFFLINE;
            case ChromiumEffectiveConnectionType.EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
                return EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
            case ChromiumEffectiveConnectionType.EFFECTIVE_CONNECTION_TYPE_2G:
                return EFFECTIVE_CONNECTION_TYPE_2G;
            case ChromiumEffectiveConnectionType.EFFECTIVE_CONNECTION_TYPE_3G:
                return EFFECTIVE_CONNECTION_TYPE_3G;
            case ChromiumEffectiveConnectionType.EFFECTIVE_CONNECTION_TYPE_4G:
                return EFFECTIVE_CONNECTION_TYPE_4G;
            case ChromiumEffectiveConnectionType.EFFECTIVE_CONNECTION_TYPE_BROADBAND:
                return EFFECTIVE_CONNECTION_TYPE_BROADBAND;
        }
        throw new IllegalStateException(
                "Effective connection type has an invalid value of " + effectiveConnectionType);
    }
}