// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;

/**
 * Tests WebApkShareTargetUtil.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {WebApkShareTargetUtilTest.WebApkShareTargetUtilShadow.class})
public class WebApkShareTargetUtilTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";

    // Android Manifest meta data for {@link PACKAGE_NAME}.
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";

    private void registerWebApk(Bundle shareTargetBundle) {
        Bundle appBundle = new Bundle();
        appBundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, appBundle, new Bundle[] {shareTargetBundle});
    }

    private static Intent createBasicShareIntent() {
        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, WEBAPK_PACKAGE_NAME);
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0));
        intent.putExtra(ShortcutHelper.EXTRA_URL, START_URL);
        return intent;
    }

    private static void assertPostData(WebApkShareTargetUtil.PostData postData, String[] names,
            String[] values, String[] fileNames, String[] types) {
        Assert.assertNotNull(postData);

        Assert.assertNotNull(postData.names);
        Assert.assertEquals(postData.names.size(), names.length);
        for (int i = 0; i < names.length; i++) {
            Assert.assertEquals(postData.names.get(i), names[i]);
        }

        Assert.assertNotNull(postData.values);
        Assert.assertEquals(postData.values.size(), values.length);
        for (int i = 0; i < values.length; i++) {
            Assert.assertEquals(new String(postData.values.get(i)), values[i]);
        }

        Assert.assertNotNull(postData.filenames);
        Assert.assertEquals(postData.filenames.size(), fileNames.length);
        for (int i = 0; i < fileNames.length; i++) {
            Assert.assertEquals(postData.filenames.get(i), fileNames[i]);
        }

        Assert.assertNotNull(postData.types);
        Assert.assertEquals(postData.types.size(), types.length);
        for (int i = 0; i < types.length; i++) {
            Assert.assertEquals(postData.types.get(i), types[i]);
        }
    }

    @Implements(WebApkShareTargetUtil.class)
    public static class WebApkShareTargetUtilShadow extends WebApkShareTargetUtil {
        @Implementation
        public static byte[] readStringFromContentUri(Uri uri) {
            return "content".getBytes();
        }

        @Implementation
        public static String getFileTypeFromContentUri(Uri uri) {
            return "image/gif";
        }

        @Implementation
        public static String getFileNameFromContentUri(Uri uri) {
            return "filename";
        }
    }

    /**
     * Test that post data is null when the share method is GET.
     */
    @Test
    public void testGET() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "GET");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_ENCTYPE, "application/x-www-form-urlencoded");
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        WebApkInfo info = WebApkInfo.create(intent);

        Assert.assertEquals(null,
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData()));

        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        registerWebApk(shareActivityBundle);
        Assert.assertNotEquals(null,
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData()));
    }

    /**
     * Test that post data for application/x-www-form-urlencoded will contain
     * the correct information in the correct place.
     */
    @Test
    public void testPostUrlEncoded() throws UnsupportedEncodingException {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(
                WebApkMetaDataKeys.SHARE_ENCTYPE, "application/x-www-form-urlencoded");
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "extra_subject");
        intent.putExtra(Intent.EXTRA_TEXT, "extra_text");
        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());

        assertPostData(postData, new String[] {"title", "text"},
                new String[] {"extra_subject", "extra_text"}, new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    /**
     * Test that multipart/form-data with no names/accepts output a null postdata.
     */
    @Test
    public void testPostMultipartWithNoNamesNoAccepts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        // Note that names and accepts are not specified
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(null);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());

        assertPostData(
                postData, new String[] {}, new String[] {}, new String[] {}, new String[] {});
    }

    /**
     * Test that multipart/form-data with no files or text specified in Intent.EXTRA_STREAM will
     * output a null postdata.
     */
    @Test
    public void testPostMultipartWithNoFilesNorText() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        // Intent.EXTRA_STREAM is not specified.
        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());

        assertPostData(
                postData, new String[] {}, new String[] {}, new String[] {}, new String[] {});
    }

    @Test
    public void testPostMultipartWithFiles() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(null);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());

        assertPostData(postData, new String[] {"name"}, new String[] {"content"},
                new String[] {"filename"}, new String[] {"image/gif"});
    }

    @Test
    public void testPostMultipartWithTexts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "share-title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_URL, "share-url");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "shared_subject_value");
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());

        assertPostData(postData, new String[] {"share-title", "share-text"},
                new String[] {"shared_subject_value", "shared_text_value"}, new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    @Test
    public void testPostMultipartWithTextsOnlyTarget() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "share-title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_URL, "share-url");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "shared_subject_value");
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());

        assertPostData(postData, new String[] {"share-title", "share-text"},
                new String[] {"shared_subject_value", "shared_text_value"}, new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    @Test
    public void testPostMultipartWithFileAndTexts() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "[\"name\"]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "share-title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_URL, "share-url");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "shared_subject_value");
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(null);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());

        assertPostData(postData, new String[] {"share-title", "share-text", "name"},
                new String[] {"shared_subject_value", "shared_text_value", "content"},
                new String[] {"", "", "filename"},
                new String[] {"text/plain", "text/plain", "image/gif"});
    }

    @Test
    public void testPostMultipartWithFileAndInValidParamNames() {
        Bundle shareActivityBundle = new Bundle();
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_ENCTYPE, "multipart/form-data");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_NAMES, "not a array");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS, "[[\"image/*\"]]");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TEXT, "share-text");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_TITLE, "share-title");
        shareActivityBundle.putString(WebApkMetaDataKeys.SHARE_PARAM_URL, "share-url");

        registerWebApk(shareActivityBundle);

        Intent intent = createBasicShareIntent();
        intent.putExtra(Intent.EXTRA_SUBJECT, "shared_subject_value");
        intent.putExtra(Intent.EXTRA_TEXT, "shared_text_value");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(null);
        intent.putExtra(Intent.EXTRA_STREAM, uris);

        WebApkInfo info = WebApkInfo.create(intent);

        WebApkShareTargetUtilShadow.PostData postData =
                WebApkShareTargetUtilShadow.computePostData(WEBAPK_PACKAGE_NAME, info.shareData());

        // with invalid name parameter from Android manifest, we ignore the file sharing part.
        assertPostData(postData, new String[] {"share-title", "share-text"},
                new String[] {"shared_subject_value", "shared_text_value"}, new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }
}
