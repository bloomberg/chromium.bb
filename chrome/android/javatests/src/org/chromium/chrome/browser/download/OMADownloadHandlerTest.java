// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.pm.PackageManager;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;
import android.test.MoreAsserts;

import org.chromium.base.test.util.Feature;

import java.io.ByteArrayInputStream;
import java.util.List;

/**
 * Tests for OMADownloadHandler class.
 */
public class OMADownloadHandlerTest extends InstrumentationTestCase {

    /**
     * Test to make sure {@link OMADownloadHandler#getSize} returns the
     * right size for OMAInfo.
     */
    @SmallTest
    @Feature({"Download"})
    public void testGetSize() {
        OMADownloadHandler.OMAInfo info = new OMADownloadHandler.OMAInfo();
        assertEquals(OMADownloadHandler.getSize(info), 0);

        info.addAttributeValue("size", "100");
        assertEquals(OMADownloadHandler.getSize(info), 100);

        info.addAttributeValue("size", "100,000");
        assertEquals(OMADownloadHandler.getSize(info), 100000);

        info.addAttributeValue("size", "100000");
        assertEquals(OMADownloadHandler.getSize(info), 100000);
    }

    /**
     * Test to make sure {@link OMADownloadHandler.OMAInfo#getDrmType} returns the
     * right DRM type.
     */
    @SmallTest
    @Feature({"Download"})
    public void testGetDrmType() {
        OMADownloadHandler.OMAInfo info = new OMADownloadHandler.OMAInfo();
        assertEquals(info.getDrmType(), null);

        info.addAttributeValue("type", "text/html");
        assertEquals(info.getDrmType(), null);

        info.addAttributeValue("type", OMADownloadHandler.OMA_DRM_MESSAGE_MIME);
        assertEquals(info.getDrmType(), OMADownloadHandler.OMA_DRM_MESSAGE_MIME);

        // Test that only the first DRM MIME type is returned.
        info.addAttributeValue("type", OMADownloadHandler.OMA_DRM_CONTENT_MIME);
        assertEquals(info.getDrmType(), OMADownloadHandler.OMA_DRM_MESSAGE_MIME);
    }

    /**
     * Test to make sure {@link OMADownloadHandler#getOpennableType} returns the
     * right MIME type.
     */
    @SmallTest
    @Feature({"Download"})
    public void testGetOpennableType() {
        PackageManager pm = getInstrumentation().getContext().getPackageManager();
        OMADownloadHandler.OMAInfo info = new OMADownloadHandler.OMAInfo();
        assertEquals(OMADownloadHandler.getOpennableType(pm, info), null);

        info.addAttributeValue(OMADownloadHandler.OMA_TYPE, "application/octet-stream");
        info.addAttributeValue(OMADownloadHandler.OMA_TYPE,
                OMADownloadHandler.OMA_DRM_MESSAGE_MIME);
        info.addAttributeValue(OMADownloadHandler.OMA_TYPE, "text/html");
        assertEquals(OMADownloadHandler.getOpennableType(pm, info), null);

        info.addAttributeValue(OMADownloadHandler.OMA_OBJECT_URI, "http://www.test.com/test.html");
        assertEquals(OMADownloadHandler.getOpennableType(pm, info), "text/html");

        // Test that only the first opennable type is returned.
        info.addAttributeValue(OMADownloadHandler.OMA_TYPE, "image/png");
        assertEquals(OMADownloadHandler.getOpennableType(pm, info), "text/html");
    }

    /**
     * Test to make sure {@link OMADownloadHandler#parseDownloadDescriptor} returns the
     * correct OMAInfo if the input is valid.
     */
    @SmallTest
    @Feature({"Download"})
    public void testParseValidDownloadDescriptor() {
        String downloadDescriptor =
                "<media xmlns=\"http://www.openmobilealliance.org/xmlns/dd\">\r\n"
                + "<DDVersion>1.0</DDVersion>\r\n"
                + "<name>test.dm</name>\r\n"
                + "<size>1,000</size>\r\n"
                + "<type>image/jpeg</type>\r\n"
                + "<garbage>this is just garbage</garbage>\r\n"
                + "<type>application/vnd.oma.drm.message</type>\r\n"
                + "<vendor>testvendor</vendor>\r\n"
                + "<description>testjpg</description>\r\n"
                + "<objectURI>http://test/test.dm</objectURI>\r\n"
                + "<nextURL>http://nexturl.html</nextURL>\r\n"
                + "</media>";
        OMADownloadHandler.OMAInfo info = OMADownloadHandler.parseDownloadDescriptor(
                new ByteArrayInputStream(downloadDescriptor.getBytes()));
        assertFalse(info.isEmpty());
        assertEquals(info.getValue(OMADownloadHandler.OMA_OBJECT_URI), "http://test/test.dm");
        assertEquals(info.getValue(OMADownloadHandler.OMA_DD_VERSION), "1.0");
        assertEquals(info.getValue(OMADownloadHandler.OMA_NAME), "test.dm");
        assertEquals(info.getValue(OMADownloadHandler.OMA_SIZE), "1,000");
        assertEquals(info.getValue(OMADownloadHandler.OMA_VENDOR), "testvendor");
        assertEquals(info.getValue(OMADownloadHandler.OMA_DESCRIPTION), "testjpg");
        assertEquals(info.getValue(OMADownloadHandler.OMA_NEXT_URL), "http://nexturl.html");
        List<String> types = info.getTypes();
        MoreAsserts.assertContentsInAnyOrder(
                types, "image/jpeg", OMADownloadHandler.OMA_DRM_MESSAGE_MIME);
    }

    /**
     * Test that {@link OMADownloadHandler#parseDownloadDescriptor} returns empty
     * result on invalid input.
     */
    @SmallTest
    @Feature({"Download"})
    public void testParseInvalidDownloadDescriptor() {
        String downloadDescriptor =
                "<media xmlns=\"http://www.openmobilealliance.org/xmlns/dd\">\r\n"
                + "</media>";
        OMADownloadHandler.OMAInfo info = OMADownloadHandler.parseDownloadDescriptor(
                new ByteArrayInputStream(downloadDescriptor.getBytes()));
        assertTrue(info.isEmpty());

        downloadDescriptor =
                "<media xmlns=\"http://www.openmobilealliance.org/xmlns/dd\">\r\n"
                + "<DDVersion>1.0</DDVersion>\r\n"
                + "<name>"
                + "<size>1,000</size>\r\n"
                + "test.dm"
                + "</name>\r\n"
                + "</media>";
        info = OMADownloadHandler.parseDownloadDescriptor(
                new ByteArrayInputStream(downloadDescriptor.getBytes()));
        assertNull(info);

        downloadDescriptor =
                "garbage"
                + "<media xmlns=\"http://www.openmobilealliance.org/xmlns/dd\">\r\n"
                + "<DDVersion>1.0</DDVersion>\r\n"
                + "</media>";
        info = OMADownloadHandler.parseDownloadDescriptor(
                new ByteArrayInputStream(downloadDescriptor.getBytes()));
        assertNull(info);
    }
}
