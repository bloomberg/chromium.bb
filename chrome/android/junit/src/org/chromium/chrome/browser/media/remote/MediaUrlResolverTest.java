// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.remote;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;

import android.net.Uri;

import org.chromium.base.CommandLine;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandler;
import java.net.URLStreamHandlerFactory;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Unit tests (run on host) for {@link org.chromium.chrome.browser.media.remote.MediaUrlResolver}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaUrlResolverTest {

    // Constants copied from MediaUrlResolver. Don't use the copies in MediaUrlResolver
    // since we want the tests to detect if these are changed or corrupted.
    private static final String COOKIES_HEADER_NAME = "Cookies";
    private static final String CORS_HEADER_NAME = "Access-Control-Allow-Origin";
    private static final String RANGE_HEADER_NAME = "Range";
    private static final String RANGE_HEADER_VALUE = "bytes: 0-65536";

    private static final String USER_AGENT_HEADER_NAME = "User-Agent";

    private static class TestDelegate implements MediaUrlResolver.Delegate {
        private final String mCookies;
        private final Uri mInputUri;

        private boolean mReturnedPlayable;
        private Uri mReturnedUri;

        TestDelegate(Uri uri, String cookies) {
            mInputUri = uri;
            mCookies = cookies;
        }

        @Override
        public String getCookies() {
            return mCookies;
        }

        @Override
        public Uri getUri() {
            return mInputUri;
        }

        @Override
        public void setUri(Uri uri, boolean playable) {
            // TODO(aberent): Auto-generated method stub
            mReturnedUri = uri;
            mReturnedPlayable = playable;
        }

        Uri getReturnedUri() {
            return mReturnedUri;
        }

        boolean isPlayable() {
            return mReturnedPlayable;
        }
    }

    private DummyUrlStreamHandler mHandler = new DummyUrlStreamHandler();

    private Map<String, String> mRequestProperties;

    private Map<String, List<String>> mReturnedHeaders;

    private URL mReturnedUrl;

    /**
     * Dummy HttpURLConnection class that returns canned headers. Also prevents the test from going
     * out to the real network.
     */
    private class DummyUrlConnection extends HttpURLConnection {

        private final URL mUrl;

        protected DummyUrlConnection(URL u) {
            super(u);
            mUrl = u;
        }

        @Override
        public void connect() throws IOException {
        }

        @Override
        public void disconnect() {
        }

        @Override
        public Map<String, List<String>> getHeaderFields() {
            return mReturnedHeaders;
        }

        @Override
        public URL getURL() {
            return mReturnedUrl == null ? mUrl : mReturnedUrl;
        }

        @Override
        public void setRequestProperty(String key, String value) {
            mRequestProperties.put(key, value);
        }

        @Override
        public boolean usingProxy() {
            return false;
        }

    }

    /**
     * Class for plugging in DummyUrlConnection
     */
    private class DummyUrlStreamHandler extends URLStreamHandler {

        @Override
        protected URLConnection openConnection(URL u) throws IOException {
            return new DummyUrlConnection(u);
        }

    }
    /**
     * Class for plugging in DummyUrlStreamHandler and hence DummyUrlConnection
     */
    private class DummyUrlStreamHandlerFactory implements URLStreamHandlerFactory {

        @Override
        public URLStreamHandler createURLStreamHandler(String protocol) {
            // If the test create the handler here then the test crashes with a
            // ClassCircularityError.
            // See https://github.com/robolectric/robolectric/issues/1688
            // TODO(aberent) Once bug fixed, create handler here.
            return mHandler;
        }

    }

    /**
     * Test method for
     * {@link org.chromium.chrome.browser.media.remote.MediaUrlResolver#MediaUrlResolver}.
     * @throws MalformedURLException
     */
    @Test
    public void testMediaUrlResolver() throws MalformedURLException {

        // Initialize the command line to avoid a crash when the code checks the logging flag.
        CommandLine.init(new String[0]);

        // Plug in the dummy networking.
        URL.setURLStreamHandlerFactory(new DummyUrlStreamHandlerFactory());

        // An empty URL isn't playable
        TestDelegate delegate = resolveUri(Uri.EMPTY, null, null, null);

        assertThat("An empty Uri remains empty", delegate.getReturnedUri(), equalTo(Uri.EMPTY));
        assertThat("An empty Uri isn't playable", delegate.isPlayable(), equalTo(false));

        // A random, non parsable, URI of unknown type is treated as playable.
        delegate = resolveUri(Uri.parse("junk"), null, null, null);

        assertThat("A junk Uri of unknown type is unchanged", delegate.getReturnedUri().toString(),
                equalTo("junk"));
        assertThat("A junk Uri of unknown type is playable", delegate.isPlayable(), equalTo(true));

        // A random, non parsable, mpeg4 URI is playable.
        delegate = resolveUri(Uri.parse("junk.mp4"), null, null, null);

        assertThat("A non-parsable mp4 Uri is unchanged", delegate.getReturnedUri().toString(),
                equalTo("junk.mp4"));
        assertThat("A non-parsable mp4 Uri is playable", delegate.isPlayable(), equalTo(true));

        // A random, non parsable, HLS URI is not playable.
        delegate = resolveUri(Uri.parse("junk.m3u8"), null, null, null);

        assertThat("A non-parsable HLS Uri is unchanged", delegate.getReturnedUri().toString(),
                equalTo("junk.m3u8"));
        assertThat("A non-parsable HLS Uri is not playable", delegate.isPlayable(), equalTo(false));

        // A random, non parsable, smoothstream URI is not playable even with CORS headers.
        HashMap<String, List<String>> corsHeaders = new HashMap<String, List<String>>();
        corsHeaders.put(CORS_HEADER_NAME, new ArrayList<String>());

        delegate = resolveUri(Uri.parse("junk.ism"), null, null, corsHeaders);

        assertThat("A non-parsable smoothstream Uri is unchanged",
                delegate.getReturnedUri().toString(),
                equalTo("junk.ism"));
        assertThat(
                "A non-parsable smoothstream Uri is not playable, "
                + "even if a CORS header is available",
                delegate.isPlayable(), equalTo(false));

        // A valid mpeg4 URI is playable and unchanged.
        Uri uri = Uri.parse("http://example.com/test.mp4");

        delegate = resolveUri(uri, null, null, null);

        assertThat("A valid mp4 Uri is unchanged", delegate.getReturnedUri(), equalTo(uri));
        assertThat("A valid mp4 Uri is playable", delegate.isPlayable(), equalTo(true));

        // Check that the correct message was sent
        assertThat("The message contained the user agent name",
                mRequestProperties.get(USER_AGENT_HEADER_NAME), equalTo("User agent"));
        assertThat("The message contained the user agent name",
                mRequestProperties.get(RANGE_HEADER_NAME), equalTo(RANGE_HEADER_VALUE));
        assertThat("The message didn't contain any cookies",
                mRequestProperties.get(COOKIES_HEADER_NAME), nullValue());

        // A valid DASH URI is unplayable without headers and unchanged.
        uri = Uri.parse("http://example.com/test.mpd");

        delegate = resolveUri(uri, null, null, null);

        assertThat("A valid mpd Uri is unchanged", delegate.getReturnedUri(), equalTo(uri));
        assertThat("A valid mpd Uri without a CORS header is unplayable", delegate.isPlayable(),
                equalTo(false));

        // Adding a CORS header makes it playable.
        delegate = resolveUri(uri, null, null, corsHeaders);

        assertThat("A valid mpd Uri is unchanged", delegate.getReturnedUri(), equalTo(uri));
        assertThat("A valid mpd Uri with a CORS header is playable", delegate.isPlayable(),
                equalTo(true));

        // Adding a random header doesn't.
        HashMap<String, List<String>> randomHeaders = new HashMap<String, List<String>>();
        randomHeaders.put("Random", new ArrayList<String>());
        delegate = resolveUri(uri, null, null, randomHeaders);

        assertThat("A valid mpd Uri is unchanged", delegate.getReturnedUri(), equalTo(uri));
        assertThat("A valid mpd Uri with a random header is not playable", delegate.isPlayable(),
                equalTo(false));

        // Check that cookies are sent correctly, and that URL updates happen correctly
        uri = Uri.parse("http://example.com/test.xxx");
        URL returnedUrl = null;

        returnedUrl = new URL("http://example.com/test.ism");

        delegate = resolveUri(uri, "Cookies!", returnedUrl, null);

        assertThat("The message contained the user agent name",
                mRequestProperties.get(USER_AGENT_HEADER_NAME), equalTo("User agent"));
        assertThat("The message contained the user agent name",
                mRequestProperties.get(RANGE_HEADER_NAME), equalTo(RANGE_HEADER_VALUE));
        assertThat("The message contained the cookies",
                mRequestProperties.get(COOKIES_HEADER_NAME), equalTo("Cookies!"));

        assertThat("The returned Url is correctly updated", delegate.getReturnedUri().toString(),
                equalTo("http://example.com/test.ism"));
        // This should not be playable since it is returned as a smoothstream URI with no CORS
        // header.
        assertThat("Whether a video is playable depends on the RETURNED URI", delegate.isPlayable(),
                equalTo(false));
    }

    private TestDelegate resolveUri(final Uri uri, final String cookies, URL returnedUrl,
            Map<String, List<String>> returnedHeaders) {
        mReturnedUrl = returnedUrl;
        mReturnedHeaders = returnedHeaders == null ? new HashMap<String, List<String>>()
                : returnedHeaders;
        mRequestProperties = new HashMap<String, String>();

        TestDelegate delegate = new TestDelegate(uri, cookies);

        MediaUrlResolver resolver = new MediaUrlResolver(delegate, "User agent");
        resolver.execute();

        Robolectric.runBackgroundTasks();

        return delegate;
    }

}
