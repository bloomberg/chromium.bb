// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.os.Parcel;
import android.test.FlakyTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeBrowserProvider.BookmarkNode;
import org.chromium.chrome.browser.ChromeBrowserProvider.Type;
import org.chromium.chrome.testshell.ChromiumTestShellTestBase;
import org.chromium.chrome.testshell.util.BookmarkUtils;

import java.util.Random;

/**
 * Tests parceling of bookmark node hierarchies used by the provider client API.
 */
public class ProviderBookmarkNodeTest extends ChromiumTestShellTestBase {
    private static final String TAG = "ProviderBookmarkNodeTest";

    Random mGenerator = new Random();
    byte[][] mImageBlobs = null;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        launchChromiumTestShellWithUrl(null);

        mImageBlobs = new byte[][] {
            BookmarkUtils.getIcon("chrome/provider/icon1.png"),
            BookmarkUtils.getIcon("chrome/provider/icon2.png"),
            BookmarkUtils.getIcon("chrome/provider/icon3.png"),
        };

        for (byte[] icon : mImageBlobs) {
            assertNotNull(icon);
        }
    }

    private static BookmarkNode parcelNode(BookmarkNode node) {
        Parcel output = Parcel.obtain();
        Parcel input = Parcel.obtain();
        node.writeToParcel(output, 0);
        byte[] bytes = output.marshall();

        input.unmarshall(bytes, 0, bytes.length);
        input.setDataPosition(0);

        return BookmarkNode.CREATOR.createFromParcel(input);
    }

    private byte[] getRandomImageBlob() {
        return mImageBlobs[mGenerator.nextInt(mImageBlobs.length)];
    }

    private static BookmarkNode createMockHierarchy() {
        // Mock hierarchy.
        // + Bookmarks
        //   - Google
        //   - Google maps
        //   + Youtube
        //     + Empty folder
        //     + Some other folder
        //       - Surprised Vader
        //     - Rickroll'D
        BookmarkNode root = new BookmarkNode(1, Type.FOLDER, "Bookmarks", null, null);
        root.addChild(new BookmarkNode(2, Type.URL, "Google", "http://www.google.com/", root));
        root.addChild(new BookmarkNode(3, Type.URL, "GoogleMaps", "http://maps.google.com/", root));

        BookmarkNode folder1 = new BookmarkNode(4, Type.FOLDER, "Youtube", null, root);
        root.addChild(folder1);
        folder1.addChild(new BookmarkNode(5, Type.FOLDER, "Empty folder", null, folder1));

        BookmarkNode folder2 = new BookmarkNode(6, Type.FOLDER, "Some other folder", null, folder1);
        folder1.addChild(folder2);

        folder1.addChild(new BookmarkNode(7, Type.URL, "RickRoll'D",
                "http://www.youtube.com/watch?v=oHg5SJYRHA0", folder1));
        folder2.addChild(new BookmarkNode(8, Type.URL, "Surprised Vader",
                "http://www.youtube.com/watch?v=9h1swNWgP8Q", folder2));
        return root;
    }

    // Returns the same mock hierarchy as createMockHierarchy, but with random favicon and
    // thumbnail information including null values.
    private BookmarkNode createMockHierarchyWithImages() {
        return addImagesRecursive(createMockHierarchy());
    }

    private BookmarkNode addImagesRecursive(BookmarkNode node) {
        node.setFavicon(mGenerator.nextBoolean() ? getRandomImageBlob() : null);
        node.setThumbnail(mGenerator.nextBoolean() ? getRandomImageBlob() : null);

        for (BookmarkNode child : node.children()) {
            addImagesRecursive(child);
        }

        return node;
    }

    private static boolean isSameHierarchy(BookmarkNode h1, BookmarkNode h2) {
        return isSameHierarchyDownwards(h1.getHierarchyRoot(), h2.getHierarchyRoot());
    }

    private static boolean isSameHierarchyDownwards(BookmarkNode n1, BookmarkNode n2) {
        if (n1 == null && n2 == null) return true;
        if (n1 == null || n2 == null) return false;
        if (!n1.equalContents(n2)) return false;
        for (int i = 0; i < n1.children().size(); ++i) {
            if (!isSameHierarchyDownwards(n1.children().get(i), n2.children().get(i))) return false;
        }
        return true;
    }

    // Tests parceling and comparing each of the nodes in the provided hierarchy.
    private boolean internalTestNodeHierarchyParceling(BookmarkNode node) {
        if (node == null) return false;

        BookmarkNode parceled = parcelNode(node);
        if (!isSameHierarchy(node, parceled)) return false;

        for (BookmarkNode child : node.children()) {
            if (!internalTestNodeHierarchyParceling(child)) return false;
        }

        return true;
    }

    /**
     * @SmallTest
     * @Feature({"Android-ContentProvider"})
     * BUG 154683
     */
    @DisabledTest
    public void testBookmarkNodeParceling() throws InterruptedException {
        assertTrue(internalTestNodeHierarchyParceling(createMockHierarchy()));
    }

    /**
     * @SmallTest
     * @Feature({"Android-ContentProvider"})
     * BUG 154683
     */
    @DisabledTest
    public void testBookmarkNodeParcelingWithImages() throws InterruptedException {
        assertTrue(internalTestNodeHierarchyParceling(createMockHierarchyWithImages()));
    }

    /**
     * @SmallTest
     * @Feature({"Android-ContentProvider"})
     * BUG 154683
     */
    @DisabledTest
    public void testSingleNodeParceling() throws InterruptedException {
        BookmarkNode node = new BookmarkNode(1, Type.URL, "Google", "http://www.google.com/", null);
        assertTrue(internalTestNodeHierarchyParceling(node));
    }

    @SmallTest
    @Feature({"Android-ContentProvider"})
    public void testInvalidHierarchy() throws InterruptedException {
        BookmarkNode root = new BookmarkNode(1, Type.FOLDER, "Bookmarks", null, null);
        root.addChild(new BookmarkNode(2, Type.URL, "Google", "http://www.google.com/", root));
        root.addChild(new BookmarkNode(2, Type.URL, "GoogleMaps", "http://maps.google.com/", root));
        assertFalse(internalTestNodeHierarchyParceling(root));
    }
}
