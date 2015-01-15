// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.net.Uri;

import org.chromium.chrome.browser.UrlUtilities;

import java.io.Serializable;

/**
 * WebsiteAddress is a robust class for storing website address, which can be a
 * fully specified origin, or just a host, or a website name pattern.
 */
public class WebsiteAddress implements Comparable<WebsiteAddress>, Serializable {
    private final String mOrigin;
    private final String mScheme;
    private final String mHost;
    private final boolean mOmitProtocolAndPort;

    private static final String HTTP_SCHEME = "http";
    private static final String SCHEME_SUFFIX = "://";
    private static final String ANY_SUBDOMAIN_PATTERN = "[*.]";

    public static WebsiteAddress create(String originOrHostOrPattern) {
        if (originOrHostOrPattern == null || originOrHostOrPattern.isEmpty()) {
            return null;
        } else if (originOrHostOrPattern.startsWith(ANY_SUBDOMAIN_PATTERN)) {
            // Pattern
            return new WebsiteAddress(null, null,
                    originOrHostOrPattern.substring(ANY_SUBDOMAIN_PATTERN.length()), true);
        } else if (originOrHostOrPattern.indexOf(SCHEME_SUFFIX) != -1) {
            // Origin
            Uri uri = Uri.parse(originOrHostOrPattern);
            return new WebsiteAddress(trimTrailingBackslash(originOrHostOrPattern),
                    uri.getScheme(),
                    uri.getHost(),
                    HTTP_SCHEME.equals(uri.getScheme())
                            && (uri.getPort() == -1 || uri.getPort() == 80));
        } else {
            // Host
            return new WebsiteAddress(null, null, originOrHostOrPattern, true);
        }
    }

    private WebsiteAddress(String origin, String scheme, String host, boolean omitProtocolAndPort) {
        mOrigin = origin;
        mScheme = scheme;
        mHost = host;
        mOmitProtocolAndPort = omitProtocolAndPort;
    }

    public String getOrigin() {
        // aaa:80 and aaa must return the same origin string.
        if (mOrigin != null && mOmitProtocolAndPort)
            return HTTP_SCHEME + SCHEME_SUFFIX + mHost;
        else
            return mOrigin;
    }

    public String getHost() {
        return mHost;
    }

    public String getTitle() {
        if (mOrigin == null || mOmitProtocolAndPort) return mHost;
        return mOrigin;
    }

    private String getDomainAndRegistry() {
        if (mOrigin != null) return UrlUtilities.getDomainAndRegistry(mOrigin, false);
        // getDomainAndRegistry works better having a protocol prefix.
        return UrlUtilities.getDomainAndRegistry(HTTP_SCHEME + SCHEME_SUFFIX + mHost, false);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof WebsiteAddress) {
            WebsiteAddress other = (WebsiteAddress) obj;
            return compareTo(other) == 0;
        }
        return false;
    }

    @Override
    public int hashCode() {
        int hash = 17;
        hash = hash * 31 + (mOrigin == null ? 0 : mOrigin.hashCode());
        hash = hash * 31 + (mScheme == null ? 0 : mScheme.hashCode());
        hash = hash * 31 + (mHost == null ? 0 : mHost.hashCode());
        return hash;
    }

    @Override
    public int compareTo(WebsiteAddress to) {
        if (this == to) return 0;
        String domainAndRegistry1 = getDomainAndRegistry();
        String domainAndRegistry2 = to.getDomainAndRegistry();
        int domainComparison = domainAndRegistry1.compareTo(domainAndRegistry2);
        if (domainComparison != 0) return domainComparison;
        // The same domain. Compare by scheme for grouping sites by scheme.
        if ((mScheme == null) != (to.mScheme == null)) return mScheme == null ? -1 : 1;
        if (mScheme != null) { // && to.mScheme != null
            int schemesComparison = mScheme.compareTo(to.mScheme);
            if (schemesComparison != 0) return schemesComparison;
        }
        // Now extract subdomains and compare them RTL.
        String[] subdomains1 = getSubdomainsList();
        String[] subdomains2 = to.getSubdomainsList();
        int position1 = subdomains1.length - 1;
        int position2 = subdomains2.length - 1;
        while (position1 >= 0 && position2 >= 0) {
            int subdomainComparison = subdomains1[position1--].compareTo(subdomains2[position2--]);
            if (subdomainComparison != 0) return subdomainComparison;
        }
        return position1 - position2;
    }

    private String[] getSubdomainsList() {
        int startIndex;
        String mAddress;
        if (mOrigin != null) {
            startIndex = mOrigin.indexOf(SCHEME_SUFFIX);
            if (startIndex == -1) return new String[0];
            startIndex += SCHEME_SUFFIX.length();
            mAddress = mOrigin;
        } else {
            startIndex = 0;
            mAddress = mHost;
        }
        int endIndex = mAddress.indexOf(getDomainAndRegistry());
        return --endIndex > startIndex
                ? mAddress.substring(startIndex, endIndex).split("\\.")
                : new String[0];
    }

    private static String trimTrailingBackslash(String origin) {
        return (origin.endsWith("/")) ? origin.substring(0, origin.length() - 1) : origin;
    }
}
