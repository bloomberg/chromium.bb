// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import android.support.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Test class that verifies that {@link ChromePreferenceKeys} conforms to its constraints:
 * - No keys are both in [keys in use] and in [deprecated keys].
 * TODO(crbug.com/1023839): Make sure new keys conform to format "Chrome.[Feature].[Key]"
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ChromePreferenceKeysTest {
    /**
     * The important test: verify that keys in {@link ChromePreferenceKeys} are not reused.
     *
     * If a key was used in the past but is not used anymore, it should be in [deprecated keys].
     * Adding the same key to [keys in use] will break this test to warn the developer.
     */
    @Test
    @SmallTest
    public void testKeysAreNotReused() {
        doTestKeysAreNotReused(ChromePreferenceKeys.createUsedKeys(),
                ChromePreferenceKeys.createDeprecatedKeysForTesting());
    }

    private void doTestKeysAreNotReused(List<String> usedList, List<String> deprecatedList) {
        // Check for duplicate keys in [keys in use].
        Set<String> usedSet = new HashSet<>(usedList);
        assertEquals(usedList.size(), usedSet.size());

        // Check for duplicate keys in [deprecated keys].
        Set<String> deprecatedSet = new HashSet<>(deprecatedList);
        assertEquals(deprecatedList.size(), deprecatedSet.size());

        // Check for keys in [deprecated keys] that are now also [keys in use]. This ensures no
        // deprecated keys are reused.
        Set<String> intersection = new HashSet<>(usedSet);
        intersection.retainAll(deprecatedSet);
        if (!intersection.isEmpty()) {
            fail("\"" + intersection.iterator().next()
                    + "\" is both in |ChromePreferenceKeys.sUsedKeys| and in "
                    + "|ChromePreferenceKeys.sDeprecatedKeys|");
        }
    }

    // Below are tests to ensure that testKeysAreNotReused() works.

    @Test
    @SmallTest
    public void testReuseCheck_emptyLists() {
        doTestKeysAreNotReused(Collections.EMPTY_LIST, Collections.EMPTY_LIST);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_duplicateKey_used() {
        doTestKeysAreNotReused(Arrays.asList("UsedKey1", "UsedKey1"),
                Arrays.asList("DeprecatedKey1", "DeprecatedKey2"));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_duplicateKey_deprecated() {
        doTestKeysAreNotReused(Arrays.asList("UsedKey1", "UsedKey2"),
                Arrays.asList("DeprecatedKey1", "DeprecatedKey1"));
    }

    @Test
    @SmallTest
    public void testReuseCheck_noIntersection() {
        doTestKeysAreNotReused(Arrays.asList("UsedKey1", "UsedKey2"),
                Arrays.asList("DeprecatedKey1", "DeprecatedKey2"));
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testReuseCheck_intersection() {
        doTestKeysAreNotReused(Arrays.asList("UsedKey1", "ReusedKey"),
                Arrays.asList("ReusedKey", "DeprecatedKey1"));
    }
}
