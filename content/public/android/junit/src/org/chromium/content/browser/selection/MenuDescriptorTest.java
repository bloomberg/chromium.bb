// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Feature;

/**
 * Unit tests for MenuDescriptor.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class MenuDescriptorTest {
    @Test
    @Feature({"TextInput"})
    public void testRemoveSameItems() throws Exception {
        MenuDescriptor d1 = new MenuDescriptor();
        d1.removeItem(1);
        MenuDescriptor d2 = new MenuDescriptor();
        d2.removeItem(1);
        Assert.assertTrue(d1.equals(d2));
    }

    @Test
    @Feature({"TextInput"})
    public void testRemoveDifferentItems() throws Exception {
        MenuDescriptor d1 = new MenuDescriptor();
        d1.removeItem(1);
        MenuDescriptor d2 = new MenuDescriptor();
        d2.removeItem(2);
        Assert.assertFalse(d1.equals(d2));
    }

    @Test
    @Feature({"TextInput"})
    public void testAddSameItems() throws Exception {
        MenuDescriptor d1 = new MenuDescriptor();
        d1.addItem(1, 1, 0, "title1", null);
        MenuDescriptor d2 = new MenuDescriptor();
        d2.addItem(1, 1, 0, "title1", null);
        Assert.assertTrue(d1.equals(d2));
    }

    @Test
    @Feature({"TextInput"})
    public void testAddDifferentItemId() throws Exception {
        MenuDescriptor d1 = new MenuDescriptor();
        d1.addItem(1, 1, 0, "title1", null);
        MenuDescriptor d2 = new MenuDescriptor();
        d2.addItem(1, 2, 0, "title1", null);
        Assert.assertFalse(d1.equals(d2));
    }

    @Test
    @Feature({"TextInput"})
    public void testAddDifferentGroupId() throws Exception {
        MenuDescriptor d1 = new MenuDescriptor();
        d1.addItem(1, 1, 0, "title1", null);
        MenuDescriptor d2 = new MenuDescriptor();
        d2.addItem(2, 1, 0, "title1", null);
        Assert.assertFalse(d1.equals(d2));
    }

    @Test
    @Feature({"TextInput"})
    public void testAddDifferentOrder() throws Exception {
        MenuDescriptor d1 = new MenuDescriptor();
        d1.addItem(1, 1, 0, "title1", null);
        MenuDescriptor d2 = new MenuDescriptor();
        d2.addItem(1, 1, 1, "title1", null);
        Assert.assertFalse(d1.equals(d2));
    }

    @Test
    @Feature({"TextInput"})
    public void testAddDifferentTitle() throws Exception {
        MenuDescriptor d1 = new MenuDescriptor();
        d1.addItem(1, 1, 0, "title1", null);
        MenuDescriptor d2 = new MenuDescriptor();
        d2.addItem(1, 1, 0, "title2", null);
        Assert.assertFalse(d1.equals(d2));
    }

    @Test
    @Feature({"TextInput"})
    public void testSecondAdditionReplacesFirst() throws Exception {
        MenuDescriptor d1 = new MenuDescriptor();
        d1.addItem(1, 1, 0, "title1", null);
        MenuDescriptor d2 = new MenuDescriptor();
        d2.addItem(2, 1, 1, "title2", null);
        // Second addition for the same item id, this time all fields are the same
        // as in d1.
        d2.addItem(1, 1, 0, "title1", null);
        Assert.assertTrue(d1.equals(d2));
    }
}
