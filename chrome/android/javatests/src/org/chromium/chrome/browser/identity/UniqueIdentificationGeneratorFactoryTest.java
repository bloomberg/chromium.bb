// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

import javax.annotation.Nullable;

public class UniqueIdentificationGeneratorFactoryTest extends InstrumentationTestCase {

    @SmallTest
    @Feature({"ChromeToMobile", "Omaha", "Sync"})
    public void testSetAndGetGenerator() {
        UniqueIdentificationGeneratorFactory.clearGeneratorMapForTest();
        UniqueIdentificationGenerator gen = new TestGenerator();
        UniqueIdentificationGeneratorFactory.registerGenerator("generator", gen, false);
        assertEquals(gen, UniqueIdentificationGeneratorFactory.getInstance("generator"));
    }

    @SmallTest
    @Feature({"ChromeToMobile", "Omaha", "Sync"})
    public void testForceCanOverrideGenerator() {
        UniqueIdentificationGeneratorFactory.clearGeneratorMapForTest();
        UniqueIdentificationGenerator gen1 = new TestGenerator();
        UniqueIdentificationGenerator gen2 = new TestGenerator();
        UniqueIdentificationGenerator gen3 = new TestGenerator();
        UniqueIdentificationGeneratorFactory.registerGenerator("generator", gen1, false);
        assertEquals(gen1, UniqueIdentificationGeneratorFactory.getInstance("generator"));
        UniqueIdentificationGeneratorFactory.registerGenerator("generator", gen2, false);
        assertEquals(gen1, UniqueIdentificationGeneratorFactory.getInstance("generator"));
        UniqueIdentificationGeneratorFactory.registerGenerator("generator", gen3, true);
        assertEquals(gen3, UniqueIdentificationGeneratorFactory.getInstance("generator"));
    }

    @SmallTest
    @Feature({"ChromeToMobile", "Omaha", "Sync"})
    public void testGeneratorNotFoundThrows() {
        UniqueIdentificationGeneratorFactory.clearGeneratorMapForTest();
        UniqueIdentificationGenerator generator = null;
        try {
            generator = UniqueIdentificationGeneratorFactory.getInstance("generator");
            fail("The generator does not exist, so factory should throw an error.");
        } catch (RuntimeException e) {
            assertEquals(null, generator);
        }
    }

    private static class TestGenerator implements UniqueIdentificationGenerator {
        @Override
        public String getUniqueId(@Nullable String salt) {
            return null;
        }
    }
}
