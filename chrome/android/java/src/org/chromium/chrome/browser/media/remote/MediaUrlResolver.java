// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import android.content.Context;
import android.net.Uri;
import android.os.AsyncTask;
import android.text.TextUtils;
import android.util.Log;

import org.apache.http.Header;
import org.apache.http.message.BasicHeader;
import org.chromium.chrome.browser.ChromiumApplication;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.Arrays;

/**
 * Resolves the final URL if it's a redirect. Works asynchronously, uses HTTP
 * HEAD request to determine if the URL is redirected.
 */
public class MediaUrlResolver extends AsyncTask<Void, Void, MediaUrlResolver.Result> {

    /**
     * The interface to get the initial URI with cookies from and pass the final
     * URI to.
     */
    public interface Delegate {
        /**
         * @return the original URL to resolve.
         */
        Uri getUri();

        /**
         * @return the cookies to fetch the URL with.
         */
        String getCookies();

        /**
         * Passes the resolved URL to the delegate.
         *
         * @param uri the resolved URL.
         */
        void setUri(Uri uri, Header[] headers);
    }


    protected static final class Result {
        private final String mUri;
        private final Header[] mRelevantHeaders;

        public Result(String uri, Header[] relevantHeaders) {
            mUri = uri;
            mRelevantHeaders =
                    relevantHeaders != null
                    ? Arrays.copyOf(relevantHeaders, relevantHeaders.length)
                    : null;
        }

        public String getUri() {
            return mUri;
        }

        public Header[] getRelevantHeaders() {
            return mRelevantHeaders != null
                    ? Arrays.copyOf(mRelevantHeaders, mRelevantHeaders.length)
                    : null;
        }
    }

    private static final String TAG = "MediaUrlResolver";

    private static final String CORS_HEADER_NAME = "Access-Control-Allow-Origin";
    private static final String COOKIES_HEADER_NAME = "Cookies";
    private static final String USER_AGENT_HEADER_NAME = "User-Agent";
    private static final String RANGE_HEADER_NAME = "Range";

    // We don't want to necessarily fetch the whole video but we don't want to miss the CORS header.
    // Assume that 64k should be more than enough to keep all the headers.
    private static final String RANGE_HEADER_VALUE = "bytes: 0-65536";

    private final Context mContext;
    private final Delegate mDelegate;

    /**
     * The constructor
     * @param context the context to use to resolve the URL
     * @param delegate The customer for this URL resolver.
     */
    public MediaUrlResolver(Context context, Delegate delegate) {
        mContext = context;
        mDelegate = delegate;
    }

    @Override
    protected MediaUrlResolver.Result doInBackground(Void... params) {
        Uri uri = mDelegate.getUri();
        String url = uri.toString();
        Header[] relevantHeaders = null;
        String cookies = mDelegate.getCookies();
        String userAgent = ChromiumApplication.getBrowserUserAgent();
        // URL may already be partially percent encoded; double percent encoding will break
        // things, so decode it before sanitizing it.
        String sanitizedUrl = sanitizeUrl(Uri.decode(url));

        // If we failed to sanitize the URL (e.g. because the host name contains underscores) then
        // HttpURLConnection won't work,so we can't follow redirections. Just try to use it as is.
        // TODO (aberent): Find out if there is a way of following redirections that is not so
        // strict on the URL format.
        if (!sanitizedUrl.equals("")) {
            HttpURLConnection urlConnection = null;
            try {
                URL requestUrl = new URL(sanitizedUrl);
                urlConnection = (HttpURLConnection) requestUrl.openConnection();
                if (!TextUtils.isEmpty(cookies)) {
                    urlConnection.setRequestProperty(COOKIES_HEADER_NAME, cookies);
                }
                urlConnection.setRequestProperty(USER_AGENT_HEADER_NAME, userAgent);
                urlConnection.setRequestProperty(RANGE_HEADER_NAME, RANGE_HEADER_VALUE);

                // This triggers resolving the URL and receiving the headers.
                urlConnection.getHeaderFields();

                url = urlConnection.getURL().toString();
                String corsHeader = urlConnection.getHeaderField(CORS_HEADER_NAME);
                if (corsHeader != null) {
                    relevantHeaders = new Header[1];
                    relevantHeaders[0] = new BasicHeader(CORS_HEADER_NAME, corsHeader);
                }
            } catch (IOException e) {
                Log.e(TAG, "Failed to fetch the final URI", e);
                url = "";
            }
            if (urlConnection != null) urlConnection.disconnect();
        }
        return new MediaUrlResolver.Result(url, relevantHeaders);
    }

    @Override
    protected void onPostExecute(MediaUrlResolver.Result result) {
        String url = result.getUri();
        Uri uri = "".equals(url) ? Uri.EMPTY : Uri.parse(url);
        mDelegate.setUri(uri, result.getRelevantHeaders());
    }

    private String sanitizeUrl(String unsafeUrl) {
        URL url;
        URI uri;
        try {
            url = new URL(unsafeUrl);
            uri = new URI(url.getProtocol(), url.getUserInfo(), url.getHost(), url.getPort(),
                    url.getPath(), url.getQuery(), url.getRef());
            return uri.toURL().toString();
        } catch (URISyntaxException syntaxException) {
            Log.w(TAG, "URISyntaxException " + syntaxException);
        } catch (MalformedURLException malformedUrlException) {
            Log.w(TAG, "MalformedURLException " + malformedUrlException);
        }
        return "";
    }
}