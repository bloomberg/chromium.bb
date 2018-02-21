// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;
import static org.junit.Assert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.chromecast.base.TestUtils.Base;
import org.chromium.chromecast.base.TestUtils.Derived;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for ScopeFactories, a utility class to construct readable objects that can be passed to
 * Observable#watch().
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ScopeFactoriesTest {
    @Test
    public void testOnEnterWithConsumer() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(ScopeFactories.onEnter((String s) -> result.add(s + ": got it!")));
        controller.set("thing");
        assertThat(result, contains("thing: got it!"));
    }

    @Test
    public void testOnEnterWithConsumerOfSuperclass() {
        Controller<Derived> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        // Compile error if generics are wrong.
        controller.watch(
                ScopeFactories.onEnter((Base base) -> result.add(base.toString() + ": got it!")));
        controller.set(new Derived());
        assertThat(result, contains("Derived: got it!"));
    }

    @Test
    public void testOnEnterWithRunnable() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(
                ScopeFactories.onEnter(() -> result.add("ignoring value, but still got it!")));
        controller.set("invisible");
        assertThat(result, contains("ignoring value, but still got it!"));
    }

    @Test
    public void testOnEnterMultipleActivations() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(ScopeFactories.onEnter(s -> result.add(s.toString())));
        controller.set("a");
        controller.set("b");
        controller.set("c");
        assertThat(result, contains("a", "b", "c"));
    }

    @Test
    public void testOnExitNotFiredIfObservableIsNotDeactivated() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(ScopeFactories.onExit((String s) -> result.add(s + ": got it!")));
        controller.set("stuff");
        assertThat(result, emptyIterable());
    }

    @Test
    public void testOnExitWithConsumer() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(ScopeFactories.onExit((String s) -> result.add(s + ": got it!")));
        controller.set("thing");
        controller.reset();
        assertThat(result, contains("thing: got it!"));
    }

    @Test
    public void testOnExitWithConsumerOfSuperclass() {
        Controller<Derived> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        // Compile error if generics are wrong.
        controller.watch(
                ScopeFactories.onExit((Base base) -> result.add(base.toString() + ": got it!")));
        controller.set(new Derived());
        controller.reset();
        assertThat(result, contains("Derived: got it!"));
    }

    @Test
    public void testOnExitWithRunnable() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(
                ScopeFactories.onExit(() -> result.add("ignoring value, but still got it!")));
        controller.set("invisible");
        controller.reset();
        assertThat(result, contains("ignoring value, but still got it!"));
    }

    @Test
    public void testOnExitMultipleActivations() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(ScopeFactories.onExit(s -> result.add(s.toString())));
        controller.set("a");
        // Implicit reset causes exit handler to fire for "a".
        controller.set("b");
        // Implicit reset causes exit handler to fire for "b".
        controller.set("c");
        assertThat(result, contains("a", "b"));
    }

    @Test
    public void testHowUsingBothOnEnterAndOnExitLooks() {
        Controller<Derived> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(ScopeFactories.onEnter((Base base) -> result.add("enter " + base)))
                .watch(ScopeFactories.onExit((Base base) -> result.add("exit " + base)))
                .watch(ScopeFactories.onEnter(() -> result.add("enter and ignore data")))
                .watch(ScopeFactories.onExit(() -> result.add("exit and ignore data")));
        controller.set(new Derived());
        controller.reset();
        assertThat(result,
                contains("enter Derived", "enter and ignore data", "exit and ignore data",
                        "exit Derived"));
    }
}
