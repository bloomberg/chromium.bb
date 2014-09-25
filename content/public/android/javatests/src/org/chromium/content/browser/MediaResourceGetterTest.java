// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.MediaMetadataRetriever;
import android.net.ConnectivityManager;
import android.test.InstrumentationTestCase;
import android.test.mock.MockContext;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.SparseArray;

import org.chromium.content.browser.MediaResourceGetter.MediaMetadata;

import java.io.File;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Tests for MediaResourceGetter.
 */
@SuppressLint("SdCardPath")
public class MediaResourceGetterTest extends InstrumentationTestCase {
    private static final String TEST_HTTP_URL = "http://example.com";
    private static final String TEST_USER_AGENT = // Anything, really
            "Mozilla/5.0 (Windows NT 6.2; Win64; x64) AppleWebKit/537.36 " +
            "(KHTML, like Gecko) Chrome/32.0.1667.0 Safari/537.36";
    private static final String TEST_FILE_PATH = "/mnt/sdcard/test";
    private static final String TEST_FILE_URL = "file://" + TEST_FILE_PATH;
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
            if (this == obj)
                return true;
            if (!super.equals(obj))
                return false;
            if (getClass() != obj.getClass())
                return false;
            FakeFile other = (FakeFile) obj;
            if (mExists != other.mExists)
                return false;
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
            assertEquals(android.Manifest.permission.ACCESS_NETWORK_STATE,
                    permission);
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

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mFakeMRG = new FakeMediaResourceGetter();
        mMockContext = new InternalMockContext();
    }

    @SmallTest
    public void testMediaMetadataEquals() {
        assertEquals(sEmptyMetadata, sEmptyMetadata);
        assertEquals(sEmptyMetadata, new MediaMetadata(0, 0, 0, false));
        assertFalse(sEmptyMetadata.equals(null));
        assertFalse(sEmptyMetadata.equals("test"));
        assertFalse(sEmptyMetadata.equals(new MediaMetadata(1, 0, 0, false)));
        assertFalse(sEmptyMetadata.equals(new MediaMetadata(0, 1, 0, false)));
        assertFalse(sEmptyMetadata.equals(new MediaMetadata(0, 0, 1, false)));
        assertFalse(sEmptyMetadata.equals(new MediaMetadata(0, 0, 0, true)));
    }

    @SmallTest
    public void testMediaMetadataHashCode() {
        assertEquals(sEmptyMetadata.hashCode(), sEmptyMetadata.hashCode());
        assertEquals(sEmptyMetadata.hashCode(), new MediaMetadata(0, 0, 0, false).hashCode());
        assertFalse(sEmptyMetadata.hashCode() == new MediaMetadata(1, 0, 0, false).hashCode());
        assertFalse(sEmptyMetadata.hashCode() == new MediaMetadata(0, 1, 0, false).hashCode());
        assertFalse(sEmptyMetadata.hashCode() == new MediaMetadata(0, 0, 1, false).hashCode());
        assertFalse(sEmptyMetadata.hashCode() == new MediaMetadata(0, 0, 0, true).hashCode());
    }

    @SmallTest
    public void testMediaMetadataGetters() {
        MediaMetadata data = new MediaMetadata(1, 2, 3, true);
        assertEquals(1, data.getDurationInMilliseconds());
        assertEquals(2, data.getWidth());
        assertEquals(3, data.getHeight());
        assertTrue(data.isSuccess());

        // Validate no overlap of test values with defaults
        data = new MediaMetadata(4, 5, 6, false);
        assertEquals(4, data.getDurationInMilliseconds());
        assertEquals(5, data.getWidth());
        assertEquals(6, data.getHeight());
        assertFalse(data.isSuccess());
    }

    @SmallTest
    public void testConfigure_Net_NoPermissions() {
        mMockContext.mAllowPermission = false;
        assertFalse(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                       TEST_COOKIES, TEST_USER_AGENT));
    }

    @SmallTest
    public void testConfigure_Net_NoActiveNetwork() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = null;
        assertFalse(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                       TEST_COOKIES, TEST_USER_AGENT));
    }

    @SmallTest
    public void testConfigure_Net_Disallowed_Mobile() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_MOBILE;
        assertFalse(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                       TEST_COOKIES, TEST_USER_AGENT));
    }

    @SmallTest
    public void testConfigure_Net_Disallowed_Wimax() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_WIMAX;
        assertFalse(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                       TEST_COOKIES, TEST_USER_AGENT));
    }

    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_Cookies_NoUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        assertTrue(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                      TEST_COOKIES, null));
        assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        assertEquals(sHeadersCookieOnly, mFakeMRG.mHeaders);
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_Net_Allowed_Wifi_Cookies_NoUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_WIFI;
        assertTrue(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                      TEST_COOKIES, null));
        assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        assertEquals(sHeadersCookieOnly, mFakeMRG.mHeaders);
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_NoCookies_NoUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        assertTrue(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                      "", null));
        assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        assertEquals(Collections.emptyMap(), mFakeMRG.mHeaders);
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_Cookies_WithUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        assertTrue(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                      TEST_COOKIES, TEST_USER_AGENT));
        assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        assertEquals(sHeadersCookieAndUA, mFakeMRG.mHeaders);
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_NoCookies_WithUA() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        assertTrue(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                      "", TEST_USER_AGENT));
        assertEquals(TEST_HTTP_URL, mFakeMRG.mUri);
        assertEquals(sHeadersUAOnly, mFakeMRG.mHeaders);
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_Net_Allowed_Ethernet_Exception() {
        mMockContext.mAllowPermission = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        mFakeMRG.mNetworkType = ConnectivityManager.TYPE_ETHERNET;
        assertFalse(mFakeMRG.configure(mMockContext, TEST_HTTP_URL,
                                       "", TEST_USER_AGENT));
        assertNull(mFakeMRG.mUri);
        assertNull(mFakeMRG.mHeaders);
    }

    @SmallTest
    public void testConfigure_Net_Allowed_LocalHost_WithNoNetwork() {
        String[] localHostUrls = {
            "http://LocalHost",
            "https://127.0.0.1/",
            "http://[::1]:8888/",
        };
        mMockContext.mAllowPermission = true;
        mFakeMRG.mNetworkType = null;
        for (String localHostUrl : localHostUrls) {
            assertTrue(mFakeMRG.configure(mMockContext, localHostUrl,
                                          TEST_COOKIES, TEST_USER_AGENT));
            assertEquals(localHostUrl, mFakeMRG.mUri);
            assertEquals(sHeadersCookieAndUA, mFakeMRG.mHeaders);
            assertNull(mFakeMRG.mPath);
        }
    }

    @SmallTest
    public void testConfigure_File_Allowed_MntSdcard() {
        final String path = "/mnt/sdcard/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        assertTrue(mFakeMRG.configure(mMockContext, url, "", null));
        assertEquals(path, mFakeMRG.mPath);
        assertNull(mFakeMRG.mUri);
        assertNull(mFakeMRG.mHeaders);
    }

    @SmallTest
    public void testConfigure_File_Allowed_Sdcard() {
        final String path = "/sdcard/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        assertTrue(mFakeMRG.configure(mMockContext, url, "", null));
        assertEquals(path, mFakeMRG.mPath);
        assertNull(mFakeMRG.mUri);
        assertNull(mFakeMRG.mHeaders);
    }

    @SmallTest
    public void testConfigure_File_Allowed_Sdcard_DoesntExist() {
        final String path = "/sdcard/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = false;
        assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_File_Allowed_ExternalStorage() {
        final String path = sExternalStorageDirectory + "/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        assertTrue(mFakeMRG.configure(mMockContext, url, "", null));
        assertEquals(path, mFakeMRG.mPath);
        assertNull(mFakeMRG.mUri);
        assertNull(mFakeMRG.mHeaders);
    }

    @SmallTest
    public void testConfigure_File_Disallowed_Innocent() {
        final String path = "/malicious/path";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_File_Disallowed_Malicious() {
        final String path = "/mnt/sdcard/../../data";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_File_Allowed_Exception() {
        final String path = "/mnt/sdcard/test";
        final String url = "file://" + path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_Blob_Disallow_Null_Cache() {
        final String path = "/data/data/" + null + "/cache/";
        final String url = path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_Blob_Disallowed_Incomplete_Path() {
        final String path = "/data/data/";
        final String url = path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_Blob_Disallowed_Unknown_Path() {
        final String path = "/unknown/path/";
        final String url = path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testConfigure_Blob_Disallowed_Other_Application() {
        final String path = "/data/data/org.some.other.app/cache/";
        final String url = path;
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInConfigure = true;
        assertFalse(mFakeMRG.configure(mMockContext, url, "", null));
        assertNull(mFakeMRG.mPath);
    }

    @SmallTest
    public void testExtract_NoMetadata() {
        mFakeMRG.mFileExists = true;
        assertEquals(sEmptyMetadata, mFakeMRG.extract(
                mMockContext, TEST_FILE_URL, null, null));
        assertEquals("configured successfully when we shouldn't have",
                     TEST_FILE_PATH, mFakeMRG.mPath); // tricky
    }

    @SmallTest
    public void testExtract_WithMetadata_ValidDuration() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        final MediaMetadata expected = new MediaMetadata(1, 0, 0, true);
        assertEquals(expected, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @SmallTest
    public void testExtract_WithMetadata_InvalidDuration() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "i am not an integer");
        assertEquals(sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_NoWidth_NoHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        assertEquals(sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_ValidWidth_NoHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH, "1");
        assertEquals(sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_InvalidWidth_NoHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH, "i am not an integer");
        assertEquals(sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_ValidWidth_ValidHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH, "2");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT, "3");
        final MediaMetadata expected = new MediaMetadata(1, 2, 3, true);
        assertEquals(expected, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_HasVideo_ValidWidth_InvalidHeight() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_HAS_VIDEO, "yes");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH, "1");
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT, "i am not an integer");
        assertEquals(sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

    @SmallTest
    public void testExtract_WithMetadata_ValidDuration_ExceptionInExtract() {
        mFakeMRG.mFileExists = true;
        mFakeMRG.mThrowExceptionInExtract = true;
        mFakeMRG.bind(MediaMetadataRetriever.METADATA_KEY_DURATION, "1");
        assertEquals(sEmptyMetadata, mFakeMRG.extract(mMockContext, TEST_FILE_URL, null, null));
    }

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
        assertEquals(expected, mFakeMRG.extract(fd, offset, length));
        assertEquals(fd, mFakeMRG.mFd);
        assertEquals(offset, mFakeMRG.mOffset);
        assertEquals(length, mFakeMRG.mLength);
    }

    @SmallTest
    public void testAndroidDeviceOk_BadModel_BadVersion() {
        assertFalse(MediaResourceGetter.androidDeviceOk(
                "GT-I9100", android.os.Build.VERSION_CODES.ICE_CREAM_SANDWICH_MR1));
    }

    @SmallTest
    public void testAndroidDeviceOk_BadModel_GoodVersion() {
        assertTrue(MediaResourceGetter.androidDeviceOk(
                "GT-I9100", android.os.Build.VERSION_CODES.JELLY_BEAN));
    }

    @SmallTest
    public void testAndroidDeviceOk_GoodModel_AnyVersion() {
        assertTrue(MediaResourceGetter.androidDeviceOk(
                "Happy Device", android.os.Build.VERSION_CODES.ICE_CREAM_SANDWICH));
    }
}
