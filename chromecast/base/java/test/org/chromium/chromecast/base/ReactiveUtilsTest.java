// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.Matchers.contains;
import static org.junit.Assert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.chromecast.base.TestUtils.Base;
import org.chromium.chromecast.base.TestUtils.Derived;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for helper methods used to work with Observables.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ReactiveUtilsTest {
    @Test
    public void testWatchBothWithStrings() {
        Controller<String> controllerA = new Controller<>();
        Controller<String> controllerB = new Controller<>();
        List<String> result = new ArrayList<>();
        ReactiveUtils.watchBoth(controllerA.and(controllerB), (String a, String b) -> {
            result.add("enter: " + a + ", " + b);
            return () -> result.add("exit: " + a + ", " + b);
        });
        controllerA.set("A");
        controllerB.set("B");
        controllerA.set("AA");
        controllerB.set("BB");
        assertThat(result,
                contains("enter: A, B", "exit: A, B", "enter: AA, B", "exit: AA, B",
                        "enter: AA, BB"));
    }

    @Test
    public void testWatchBothWithSuperclassAsScopeFactoryParameters() {
        Controller<Derived> controllerA = new Controller<>();
        Controller<Derived> controllerB = new Controller<>();
        List<String> result = new ArrayList<>();
        // Compile error if generics are wrong.
        ReactiveUtils.watchBoth(controllerA.and(controllerB), (Base a, Base b) -> {
            result.add("enter: " + a + ", " + b);
            return () -> result.add("exit: " + a + ", " + b);
        });
        controllerA.set(new Derived());
        controllerB.set(new Derived());
        assertThat(result, contains("enter: Derived, Derived"));
    }
}
