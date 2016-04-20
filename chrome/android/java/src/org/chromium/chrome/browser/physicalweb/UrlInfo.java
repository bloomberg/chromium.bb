// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.Locale;

/**
 * This class represents a scanned URL and information associated with that URL.
 */
class UrlInfo implements Comparable<UrlInfo> {
    private static final String URL_KEY = "url";
    private static final String DISTANCE_KEY = "distance";
    private final String mUrl;
    private final double mDistance;

    public UrlInfo(String url, double distance) {
        mUrl = url;
        mDistance = distance;
    }

    /**
     * Constructs a simple UrlInfo with only a URL.
     */
    public UrlInfo(String url) {
        this(url, -1.0);
    }

    /**
     * Gets the URL represented by this object.
     * @param The URL.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * Gets the distance of the URL from the scanner in meters..
     * @return The estimated distance of the URL from the scanner in meters.
     */
    public double getDistance() {
        return mDistance;
    }

    /**
     * Creates a JSON object that represents this data structure.
     * @return a JSON serialization of this data structure.
     * @throws JSONException if the values cannot be deserialized.
     */
    public JSONObject jsonSerialize() throws JSONException {
        return new JSONObject()
                .put(URL_KEY, mUrl)
                .put(DISTANCE_KEY, mDistance);
    }

    /**
     * Populates a UrlInfo with data from a given JSON object.
     * @param jsonObject a serialized UrlInfo.
     * @return The UrlInfo represented by the serialized object.
     * @throws JSONException if the values cannot be serialized.
     */
    public static UrlInfo jsonDeserialize(JSONObject jsonObject) throws JSONException {
        return new UrlInfo(jsonObject.getString(URL_KEY), jsonObject.getDouble(DISTANCE_KEY));
    }

    /**
     * Returns a hash code for this UrlInfo.
     * @return hash code
     */
    @Override
    public int hashCode() {
        int hash = 31 + mUrl.hashCode();
        hash = hash * 31 + (int) mDistance;
        return hash;
    }

    /**
     * Checks if two UrlInfos are equal.
     * @param other the UrlInfo to compare to.
     * @return true if the UrlInfo are equal.
     */
    @Override
    public boolean equals(Object other) {
        if (this == other) {
            return true;
        }

        if (other instanceof UrlInfo) {
            UrlInfo urlInfo = (UrlInfo) other;
            return compareTo(urlInfo) == 0;
        }
        return false;
    }

    /**
     * Compares two UrlInfos.
     * @param other the UrlInfo to compare to.
     * @return the comparison value.
     */
    @Override
    public int compareTo(UrlInfo other) {
        int compareValue = mUrl.compareTo(other.mUrl);
        if (compareValue != 0) {
            return compareValue;
        }

        compareValue = Double.compare(mDistance, other.mDistance);
        if (compareValue != 0) {
            return compareValue;
        }

        return 0;
    }

    /**
     * Represents the UrlInfo as a String.
     */
    @Override
    public String toString() {
        return String.format(Locale.getDefault(), "%s %f", mUrl, mDistance);
    }
}
