// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.MediaMetadataRetriever;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.support.test.filters.SmallTest;
import android.test.mock.MockContext;
import android.util.SparseArray;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content.browser.MediaResourceGetter.MediaMetadata;

import java.io.File;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Tests for MediaResourceGetter.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@SuppressLint("SdCardPath")
public class MediaResourceGetterTest {
    private static final String TEST_HTTP_URL = "http://example.com";
    private static final String TEST_USER_AGENT = // Anything, really
            "Mozilla/5.0 (Windows NT 6.2; Win64; x64) AppleWebKit/537.36 "
            + "(KHTML, like Gecko) Chrome/32.0.1667.0 Safari/537.36";
    private static final String TEST_FILE_PATH = "/mnt/sdcard/test";
    private static final String TEST_FILE_URL = "file://" + TEST_FILE_PATH;
    private static final String TEST_CONTENT_URI =
            "content://com.android.providers.media.documents/document/video:113";
    private static final String TEST_COOKIES = "yum yum yum!";
    private static final MediaMetadata sEmptyMetadata = new MediaMetadata(0, 0, 0, false);
    private static final String sExternalStorageDirectory = "/test_external_storage";

    private static final Map<String, String> sHeadersCookieOnly;
    private static final Map<String, String> sHeadersCookieAndUA;
    private static final Map<String, String> sHeadersUAOnly;
    static {
        Map<String, String> headers = new HashMap<String, String>();
        headers.put("Cookie", TEST_COOKIES);
        sHeadersCookieOnly = Collections.unmodifiableMap(headers);

        headers = new HashMap<String, String>();
        headers.put("User-Agent", TEST_USER_AGENT);
        sHeadersUAOnly = Collections.unmodifiableMap(headers);

        headers = new HashMap<String, String>();
        headers.putAll(sHeadersCookieOnly);
        headers.putAll(sHeadersUAOnly);
        sHeadersCookieAndUA = Collections.unmodifiableMap(headers);
    }

    private static class FakeFile extends File {
        final boolean mExists;

        public FakeFile(String path, boolean exists) {
            super(path);
            mExists = exists;
        }

        @Override
        public int hashCode() {
            final int prime = 31;
            int result = super.hashCode();
            result = prime * result + (mExists ? 1231 : 1237);
            return result;
        }

        @Override
        public boolean equals(Object obj) {
            if (this == obj) return true;
            if (!super.equals(obj)) return false;
            if (getClass() != obj.getClass()) return false;
            FakeFile other = (FakeFile) obj;
            if (mExists != other.mExists) return false;
            return true;
        }

        @Override
        public boolean exists() {
            return mExists;
        }
    }

    /**
     * Helper class that fakes functionality we cannot perform without real
     * media resources. The class being faked here has been architected
     * carefully to allow most of the functionality to be tested. What remains
     * here is overrides of trivial functionality.
     */
    private static class FakeMediaResourceGetter extends MediaResourceGetter {
        // Read these back in tests to ensure proper values passed through
        String mUri = null;
        Map<String, String> mHeaders = null;
        String mPath = null;
        String mContentUri = null;
        int mFd;
        long mOffset;
        long mLength;

        // Write these before tests to configure functionality
        SparseArray<String> mMetadata = null;
        Integer mNetworkType = null;
        boolean mThrowExceptionInConfigure = false;
        boolean mThrowExceptionInExtract = false;
        boolean mFileExists = false;

        // Can't use a real MediaMetadataRetriever as we have no media
        @Override
        public void configure(int fd, long offset, long length) {
            if (mThrowExceptionInConfigure) {
                throw new RuntimeException("test exception");
            }
            mFd = fd;
            mOffset = offset;
            mLength = length;
        }

        // Can't use a real MediaMetadataRetriever as we have no media
        @Override
        public void configure(String uri, Map<String, String> headers) {
            if (mThrowExceptionInConfigure) {
                throw new RuntimeException("test exception");
            }
            mUri = uri;
            mHeaders = headers;
        }

        // Can't use a real MediaMetadataRetriever as we have no media
        @Override
        public void configure(String path) {
            if (mThrowExceptionInConfigure) {
                throw new RuntimeException("test exception");
            }
            mPath = path;
        }

        // Can't use a real MediaMetadataRetriever as we have no media
        @Override
        public void configure(Context context, Uri uri) {
            if (mThrowExceptionInConfigure) {
                throw new RuntimeException("test exception");
            }
            mContentUri = uri.toString();
        }

        // Can't use a real MediaMetadataRetriever as we have no media
        @Override
        public String extractMetadata(int key) {
            if (mThrowExceptionInExtract) {
                throw new RuntimeException("test exception");
            }
            if (mMetadata == null) {
                return null;
            }
            return mMetadata.get(key);
        }

        // Can't use a real NetworkInfo object because there is no way to
        // mock the ConnectivityManager in Android.
        @Override
        Integer getNetworkType(Context context) {
            return mNetworkType;
        }

        // Can't use Environment.getExternalStorageDirectory because it could
        // be inconsistent depending upon the state of the real or emulated
        // device upon which we are testing.
        @Override
        String getExternalStorageDirectory() {
            return sExternalStorageDirectory;
        }

        // Can't use regular File because we need to control the return from
        // File.exists() on arbitrary paths.
        @Override
        File uriToFile(String path) {
            FakeFile result = new FakeFile(path, mFileExists);
            return result;
        }

        /**
         * Convenience method to bind a metadata value to a key.
         * @param key the key
         * @param value the value
         */
        void bind(int key, String value) {
            if (mMetadata == null) {
                mMetadata = new SparseArray<String>();
            }
            mMetadata.put(key, value);
        }

    }

    /**
     * Helper class to control the result of permission checks.
     */
    private static class InternalMockContext extends MockContext {
        boolean mAllowPermission = false;
        @Override
        public int checkCallingOrSelfPermission(String permission) {
            Assert.assertEquals(android.Manifest.permission.ACCESS_NETWORK_STATE, permission);
            return mAllowPermission ? PackageManager.PERMISSION_GRANTED :
                PackageManager.PERMISSION_DENIED;
        }

        @Override
        public String getPackageName() {
            return "org.some.app.package";
        }
    }

    // Our test objects.
    private FakeMediaResourceGetter mFakeMRG;
    private InternalMockContext mMockContext;

    @Before
    public void setUp() throws Exception {
        mFakeMRG = new FakeMediaResourceGetter();
        mMockContext = new InternalMockContext();
    }

    @Test
    @SmallTest
    public void testMediaMetadataEquals() {
        Assert.assertEquals(sEmptyMetadata, sEmptyMetadata);
        Assert.assertEquals(sEmptyMetadata, new MediaMetadata(0, 0, 0, false));
        Assert.assertFalse(sEmptyMetadata.equals(null));
        Assert.assertFalse(sEmptyMetadata.equals("test"));
        Assert.assertFalse(sEmptyMetadata.equals(new MediaMetadata(1, 0, 0, false)));
        Assert.assertFalse(sEmptyMetadata.equals(new MediaMetadata(0, 1, 0, false)));
        Assert.assertFalse(sEmptyMetadata.equals(new MediaMetadata(0, 0, 1, false)));
        Assert.assertFalse(sEmptyMetadata.equals(new MediaMetadata(0, 0, 0, true)));
    }

    @Test
    @SmallTest
    public void testMediaMetadataHashCode() {
        Assert.assertEquals(sEmptyMetadata.hashCode(), sEmptyMetadata.hashCode());
        Assert.assertEquals(
                sEmptyMetadata.hashCode(), new MediaMetadata(0, 0, 0, false).hashCode());
        Assert.assertFalse(
                sEmptyMetadata.hashCode() == new MediaMetadata(1, 0, 0, false).hashCode());
        Assert.assertFalse(
                sEmptyMetadata.hashCode() == new MediaMetadata(0, 1, 0, false).hashCode());
        Assert.assertFalse(
                sEmptyMetadata.hashCode() == new MediaMetadata(0, 0, 1, false).hashCode());
        Assert.assertFalse(
                sEmptyMetadata.hashCode() == new MediaMetadata(0, 0, 0, true).hashCode());
    }

    @Test
    @SmallTest
    public void testMediaMetadataGetters() {
        MediaMetadata data = new MediaMetadata(1, 2, 3, true);
        Assert.assertEquals(1, data.getDurationInMilliseconds());
        Assert.assertEquals(2, data.getWidth());
        Assert.assertEquals(3, data.getHeight());
        Assert.assertTrue(data.isSuccess());

        // Validate no overlap of test values with defaults
        data = new MediaMetadata(4, 5, 6, false);
        Assert.assertEquals(4, data.getDurationInMilliseconds());
        Assert.assertEquals(5, data.getWidth());
        Assert.assertEquals(6, data.getHeight());
        Assert.assertFalse(data.isSuccess());
    }

    @Test
    @SmallTest
    public void testConfigure_Net_NoPermissions() {
        mMockContext.mAllowPermission = false;
        Assert.assertFalse(
                mFakeMRG.configure(mMockContext, TEST_HTTP_URL, TEST_COOKIES, TEST_USER_AGENT));
    }

    @Test
    @SmallTest
    public void testConfigure_Net_NoActiveNetwork() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = null;
        Assert.assertFalse(
                mFakeMRG.configure(mMockContext, TEST_HTTP_URL, TEST_COOKIES, TEST_USER_AGENT));
    }

    @Test
    @SmallTest
    public void testConfigure_Net_Disallowed_Mobile() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_MOBILE;
        Assert.assertFalse(
                mFakeMRG.configure(mMockContext, TEST_HTTP_URL, TEST_COOKIES, TEST_USER_AGENT));
    }

    @Test
    @SmallTest
    public void testConfigure_Net_Disallowed_Wimax() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_WIMAX;
        Assert.assertFalse(
                mFakeMRG.configure(mMockContext, TEST_HTTP_URL, TEST_COOKIES, TEST_USER_AGENT));
    }

    @Test
    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_Cookies_NoUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        Assert.assertTrue(mFakeMRG.configure(mMockContext, TEST_HTTP_URL, TEST_COOKIES, null));
        Assert.assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        Assert.assertEquals(sHeadersCookieOnly, mFakeMRG.mHeaders);
        Assert.assertNull(mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mContentUri);
    }

    @Test
    @SmallTest
    public void testConfigure_Net_Allowed_Wifi_Cookies_NoUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_WIFI;
        Assert.assertTrue(mFakeMRG.configure(mMockContext, TEST_HTTP_URL, TEST_COOKIES, null));
        Assert.assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        Assert.assertEquals(sHeadersCookieOnly, mFakeMRG.mHeaders);
        Assert.assertNull(mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mContentUri);
    }

    @Test
    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_NoCookies_NoUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        Assert.assertTrue(mFakeMRG.configure(mMockContext, TEST_HTTP_URL, "", null));
        Assert.assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        Assert.assertEquals(Collections.emptyMap(), mFakeMRG.mHeaders);
        Assert.assertNull(mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mContentUri);
    }

    @Test
    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_Cookies_WithUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        Assert.assertTrue(
                mFakeMRG.configure(mMockContext, TEST_HTTP_URL, TEST_COOKIES, TEST_USER_AGENT));
        Assert.assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        Assert.assertEquals(sHeadersCookieAndUA, mFakeMRG.mHeaders);
        Assert.assertNull(mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mContentUri);
    }

    @Test
    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_NoCookies_WithUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        Assert.assertTrue(mFakeMRG.configure(mMockContext, TEST_HTTP_URL, "", TEST_USER_AGENT));
        Assert.assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        Assert.assertEquals(sHeadersUAOnly, mFakeMRG.mHeaders);
        Assert.assertNull(mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mContentUri);
    }

    @Test
    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_Exception() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, TEST_HTTP_URL, "", TEST_USER_AGENT));
        Assert.assertNull(mFakeMRG.mUri);
        Assert.assertNull(mFakeMRG.mHeaders);
    }

    @Test
    @SmallTest
    public void testConfigure_Net_Allowed_LocalHost_WithNoNetwork() {
        String[] localHostUrls = {
            "http://LocalHost",
            "http://LocalHost6",
            "http://helloworld.localhost:8000",
            "https://127.0.0.1/",
            "http://[::1]:8888/",
        };
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = null;
        for (String localHostUrl : localHostUrls) {
            Assert.assertTrue(
                    mFakeMRG.configure(mMockContext, localHostUrl, TEST_COOKIES, TEST_USER_AGENT));
            Assert.assertEquals(localHostUrl, mFakeMRG.mUri);
            Assert.assertEquals(sHeadersCookieAndUA, mFakeMRG.mHeaders);
            Assert.assertNull(mFakeMRG.mPath);
            Assert.assertNull(mFakeMRG.mContentUri);
        }
    }

    @Test
    @SmallTest
    public void testConfigure_File_Allowed_MntSdcard() {
        final String path = "/mnt/sdcard/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        Assert.assertTrue(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertEquals(path, mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mUri);
        Assert.assertNull(mFakeMRG.mContentUri);
        Assert.assertNull(mFakeMRG.mHeaders);
    }

    @Test
    @SmallTest
    public void testConfigure_File_Allowed_Sdcard() {
        final String path = "/sdcard/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        Assert.assertTrue(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertEquals(path, mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mUri);
        Assert.assertNull(mFakeMRG.mContentUri);
        Assert.assertNull(mFakeMRG.mHeaders);
    }

    @Test
    @SmallTest
    public void testConfigure_File_Allowed_Sdcard_DoesntExist() {
        final String path = "/sdcard/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = false;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertNull(mFakeMRG.mPath);
    }

    @Test
    @SmallTest
    public void testConfigure_File_Allowed_ExternalStorage() {
        final String path = sExternalStorageDirectory + "/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        Assert.assertTrue(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertEquals(path, mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mUri);
        Assert.assertNull(mFakeMRG.mContentUri);
        Assert.assertNull(mFakeMRG.mHeaders);
    }

    @Test
    @SmallTest
    public void testConfigure_File_Disallowed_Innocent() {
        final String path = "/malicious/path";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertNull(mFakeMRG.mPath);
    }

    @Test
    @SmallTest
    public void testConfigure_File_Disallowed_Malicious() {
        final String path = "/mnt/sdcard/../../data";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertNull(mFakeMRG.mPath);
    }

    @Test
    @SmallTest
    public void testConfigure_File_Allowed_Exception() {
        final String path = "/mnt/sdcard/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertNull(mFakeMRG.mPath);
    }

    @Test
    @SmallTest
    public void testConfigure_Blob_Disallow_Null_Cache() {
        final String path = "/data/data/" + null + "/cache/";
        final String url = path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertNull(mFakeMRG.mPath);
    }

    @Test
    @SmallTest
    public void testConfigure_Blob_Disallowed_Incomplete_Path() {
        final String path = "/data/data/";
        final String url = path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertNull(mFakeMRG.mPath);
    }

    @Test
    @SmallTest
    public void testConfigure_Blob_Disallowed_Unknown_Path() {
        final String path = "/unknown/path/";
        final String url = path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertNull(mFakeMRG.mPath);
    }

    @Test
    @SmallTest
    public void testConfigure_Blob_Disallowed_Other_Application() {
        final String path = "/data/data/org.some.other.app/cache/";
        final String url = path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        Assert.assertNull(mFakeMRG.mPath);
    }

    @Test
    @SmallTest
    public void testConfigure_Content_Uri_Allowed() {
        Assert.assertTrue(mFakeMRG.configure(mMockContext, TEST_CONTENT_URI, "", null));
        Assert.assertNull(mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mUri);
        Assert.assertEquals(TEST_CONTENT_URI, mFakeMRG.mContentUri);
    }

    @Test
    @SmallTest
    public void testConfigure_Content_Uri_Disallowed() {
        mFakeMRG.mThrowExceptionInConfigure = true;
        Assert.assertFalse(mFakeMRG.configure(mMockContext, TEST_CONTENT_URI, "", null));
        Assert.assertNull(mFakeMRG.mPath);
        Assert.assertNull(mFakeMRG.mUri);
        Assert.assertNull(mFakeMRG.mContentUri);
    }

    @Test
    @SmallTest
    public void testExtract_NoMetadata() {
        mFakeMRG.mFileExists = true;
        Assert.assertEquals(
                sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
        Assert.assertEquals("configured successfully when we shouldn't have", TEST_FILE_PATH,
                mFakeMRG.mPath); // tricky
    }

    @Test
    @SmallTest
    public void testExtract_WithMetadata_ValidDuration() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        final MediaMetadata expected = new MediaMetadata(1, 0, 0, true);
        Assert.assertEquals(expected, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @Test
    @SmallTest
    public void testExtract_WithMetadata_InvalidDuration() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "i am not an integer");
        Assert.assertEquals(
                sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @Test
    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_NoWidth_NoHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        Assert.assertEquals(
                sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @Test
    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_ValidWidth_NoHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH, "1");
        Assert.assertEquals(
                sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @Test
    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_InvalidWidth_NoHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH, "i am not an integer");
        Assert.assertEquals(
                sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @Test
    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_ValidWidth_ValidHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH, "2");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT, "3");
        final MediaMetadata expected = new MediaMetadata(1, 2, 3, true);
        Assert.assertEquals(expected, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @Test
    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_ValidWidth_InvalidHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT, "i am not an integer");
        Assert.assertEquals(
                sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @Test
    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_ExceptionInExtract() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInExtract = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        Assert.assertEquals(
                sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @Test
    @SmallTest
    public void testExtractFromFileDescriptor_ValidMetadata() {
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH, "2");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT, "3");
        final MediaMetadata expected = new MediaMetadata(1, 2, 3, true);
        int fd = 1234;
        long offset = 1000;
        long length = 9000;
        Assert.assertEquals(expected, mFakeMRG.extract(fd, offset, length));
        Assert.assertEquals(fd, mFakeMRG.mFd);
        Assert.assertEquals(offset, mFakeMRG.mOffset);
        Assert.assertEquals(length, mFakeMRG.mLength);
    }
}
