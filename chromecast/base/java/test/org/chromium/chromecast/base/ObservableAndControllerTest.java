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
 * Tests for Observable and Controller.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ObservableAndControllerTest {
    // Convenience method to create a scope that mutates a list of strings on state transitions.
    // When entering the state, it will append "enter ${id} ${data}" to the result list, where
    // `data` is the String that is associated with the state activation. When exiting the state,
    // it will append "exit ${id}" to the result list. This provides a readable way to track and
    // verify the behavior of observers in response to the Observables they are linked to.
    public static <T> ScopeFactory<T> report(List<String> result, String id) {
        // Did you know that lambdas are awesome.
        return (T data) -> {
            result.add("enter " + id + ": " + data);
            return () -> result.add("exit " + id);
        };
    }

    @Test
    public void testNoStateTransitionAfterRegisteringWithInactiveController() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(report(result, "a"));
        assertThat(result, emptyIterable());
    }

    @Test
    public void testStateIsEnteredWhenControllerIsSet() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(report(result, "a"));
        // Activate the state by setting the controller.
        controller.set("cool");
        assertThat(result, contains("enter a: cool"));
    }

    @Test
    public void testBasicStateFromController() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(report(result, "a"));
        controller.set("fun");
        // Deactivate the state by resetting the controller.
        controller.reset();
        assertThat(result, contains("enter a: fun", "exit a"));
    }

    @Test
    public void testSetStateTwicePerformsImplicitReset() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(report(result, "a"));
        // Activate the state for the first time.
        controller.set("first");
        // Activate the state for the second time.
        controller.set("second");
        // If set() is called without a reset() in-between, the tracking state exits, then re-enters
        // with the new data. So we expect to find an "exit" call between the two enter calls.
        assertThat(result, contains("enter a: first", "exit a", "enter a: second"));
    }

    @Test
    public void testResetWhileStateIsNotEnteredIsNoOp() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(report(result, "a"));
        controller.reset();
        assertThat(result, emptyIterable());
    }

    @Test
    public void testStateObservingState() {
        // Use the closure of watch() under Observable to construct two observers.
        // Verify both observers' events are triggered by the original Controller.
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable<String> state = controller.watch(report(result, "a"));
        Observable<String> metastate = state.watch(report(result, "b"));
        // Activate the controller, which should propagate a state transition to both states.
        // Both states should be updated, so we should get two enter events.
        controller.set("groovy");
        controller.reset();
        // Note that exit handlers are done in the reverse order that they were registered.
        // i.e. the last observer to get activated is the first observer to get deactivated.
        assertThat(result, contains("enter a: groovy", "enter b: groovy", "exit b", "exit a"));
    }

    @Test
    public void testMultipleStatesObservingSingleController() {
        // Construct two states that watch the same Controller. Verify both observers' events are
        // triggered.
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable<String> a = controller.watch(report(result, "a"));
        Observable<String> b = controller.watch(report(result, "b"));
        // Activate the controller, which should propagate a state transition to both states.
        // Both states should be updated, so we should get two enter events.
        controller.set("neat");
        controller.reset();
        assertThat(result, contains("enter a: neat", "enter b: neat", "exit b", "exit a"));
    }

    @Test
    public void testNewStateIsActivatedImmediatelyIfObservingAlreadyActiveObservable() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.set("surprise");
        controller.watch(report(result, "a"));
        assertThat(result, contains("enter a: surprise"));
    }

    @Test
    public void testNewStateIsNotActivatedIfObservingObservableThatHasBeenDeactivated() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.set("surprise");
        controller.reset();
        controller.watch(report(result, "a"));
        assertThat(result, emptyIterable());
    }

    @Test
    public void testResetWhileAlreadyDeactivatedIsANoOp() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.watch(report(result, "a"));
        controller.set("radical");
        controller.reset();
        // Resetting again after already resetting should not notify the observer.
        controller.reset();
        assertThat(result, contains("enter a: radical", "exit a"));
    }

    @Test
    public void testBothState_activateFirstDoesNotTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        List<String> result = new ArrayList<>();
        a.and(b).watch(report(result, "both"));
        a.set("A");
        assertThat(result, emptyIterable());
    }

    @Test
    public void testBothState_activateSecondDoesNotTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        List<String> result = new ArrayList<>();
        a.and(b).watch(report(result, "both"));
        b.set("B");
        assertThat(result, emptyIterable());
    }

    @Test
    public void testBothState_activateBothTriggers() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        List<String> result = new ArrayList<>();
        a.and(b).watch(report(result, "both"));
        a.set("A");
        b.set("B");
        assertThat(result, contains("enter both: A, B"));
    }

    @Test
    public void testBothState_deactivateFirstAfterTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        List<String> result = new ArrayList<>();
        a.and(b).watch(report(result, "both"));
        a.set("A");
        b.set("B");
        a.reset();
        assertThat(result, contains("enter both: A, B", "exit both"));
    }

    @Test
    public void testBothState_deactivateSecondAfterTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        List<String> result = new ArrayList<>();
        a.and(b).watch(report(result, "both"));
        a.set("A");
        b.set("B");
        b.reset();
        assertThat(result, contains("enter both: A, B", "exit both"));
    }

    @Test
    public void testBothState_resetFirstBeforeSettingSecond_doesNotTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        List<String> result = new ArrayList<>();
        a.and(b).watch(report(result, "both"));
        a.set("A");
        a.reset();
        b.set("B");
        assertThat(result, emptyIterable());
    }

    @Test
    public void testBothState_resetSecondBeforeSettingFirst_doesNotTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        List<String> result = new ArrayList<>();
        a.and(b).watch(report(result, "both"));
        b.set("B");
        b.reset();
        a.set("A");
        assertThat(result, emptyIterable());
    }

    @Test
    public void testBothState_setOneControllerAfterTrigger_implicitlyResetsAndSets() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        List<String> result = new ArrayList<>();
        a.and(b).watch(report(result, "both"));
        a.set("A1");
        b.set("B1");
        a.set("A2");
        b.set("B2");
        assertThat(result,
                contains("enter both: A1, B1", "exit both", "enter both: A2, B1", "exit both",
                        "enter both: A2, B2"));
    }

    @Test
    public void testComposeBoth() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        Controller<String> c = new Controller<>();
        Controller<String> d = new Controller<>();
        List<String> result = new ArrayList<>();
        a.and(b).and(c).and(d).watch(report(result, "all four"));
        a.set("A");
        b.set("B");
        c.set("C");
        d.set("D");
        a.reset();
        assertThat(result, contains("enter all four: A, B, C, D", "exit all four"));
    }

    @Test
    public void testTransform() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.watch(report(result, "unchanged"))
                .transform(String::toLowerCase)
                .watch(report(result, "lower"))
                .transform(String::toUpperCase)
                .watch(report(result, "upper"));
        a.set("sImPlY sTeAmEd KaLe");
        assertThat(result,
                contains("enter unchanged: sImPlY sTeAmEd KaLe", "enter lower: simply steamed kale",
                        "enter upper: SIMPLY STEAMED KALE"));
    }

    @Test
    public void testSetControllerWithNullImplicitlyResets() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.watch(report(result, "controller"));
        a.set("not null");
        a.set(null);
        assertThat(result, contains("enter controller: not null", "exit controller"));
    }

    @Test
    public void testResetControllerInActivationHandler() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.watch((String s) -> {
            result.add("enter " + s);
            a.reset();
            result.add("after reset");
            return () -> {
                result.add("exit");
            };
        });
        a.set("immediately retracted");
        assertThat(result, contains("enter immediately retracted", "after reset", "exit"));
    }

    @Test
    public void testSetControllerInActivationHandler() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.watch(report(result, "weirdness")).watch((String s) -> {
            // If the activation handler always calls set() on the source controller, you will have
            // an infinite loop, which is not cool. However, if the activation handler only
            // conditionally calls set() on its source controller, then the case where set() is not
            // called will break the loop. It is the responsibility of the programmer to solve the
            // halting problem for activation handlers.
            if (s.equals("first")) {
                a.set("second");
            }
            return () -> {
                result.add("haha");
            };
        });
        a.set("first");
        assertThat(result,
                contains("enter weirdness: first", "haha", "exit weirdness",
                        "enter weirdness: second"));
    }

    @Test
    public void testResetControllerInDeactivationHandler() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.watch(report(result, "bizzareness")).watch((String s) -> () -> a.reset());
        a.set("yo");
        a.reset();
        // The reset() called by the deactivation handler should be a no-op.
        assertThat(result, contains("enter bizzareness: yo", "exit bizzareness"));
    }

    @Test
    public void testSetControllerInDeactivationHandler() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.watch(report(result, "astoundingness")).watch((String s) -> () -> a.set("never mind"));
        a.set("retract this");
        a.reset();
        // The set() called by the deactivation handler should immediately set the controller back.
        assertThat(result,
                contains("enter astoundingness: retract this", "exit astoundingness",
                        "enter astoundingness: never mind"));
    }

    @Test
    public void testBeingTooCleverWithScopeFactoriesAndInheritance() {
        Controller<Base> baseController = new Controller<>();
        Controller<Derived> derivedController = new Controller<>();
        List<String> result = new ArrayList<>();
        // Test that the same ScopeFactory object can observe Observables of different types, as
        // long as the ScopeFactory type is a superclass of both Observable types.
        ScopeFactory<Base> scopeFactory = (Base value) -> {
            result.add("enter: " + value.toString());
            return () -> result.add("exit: " + value.toString());
        };
        baseController.watch(scopeFactory);
        // Compile error if generics are wrong.
        derivedController.watch(scopeFactory);
        baseController.set(new Base());
        // The scope from the previous set() call will not be overridden because this is activating
        // a different Controller.
        derivedController.set(new Derived());
        // The Controller<Base> can be activated with an object that extends Base.
        baseController.set(new Derived());
        assertThat(
                result, contains("enter: Base", "enter: Derived", "exit: Base", "enter: Derived"));
    }

    // Any AutoCloseable's constructor whose parameters match the scope can be used as a method
    // reference.
    private static class TransitionLogger implements AutoCloseable {
        public static final List<String> sResult = new ArrayList<>();
        private final String mData;

        public TransitionLogger(String data) {
            mData = data;
            sResult.add("enter: " + mData);
        }

        @Override
        public void close() {
            sResult.add("exit: " + mData);
        }
    }

    @Test
    public void testScopeFactoryWithAutoCloseableConstructor() {
        Controller<String> controller = new Controller<>();
        // You can use a constructor method reference in a watch() call.
        controller.watch(TransitionLogger::new);
        controller.set("a");
        controller.reset();
        assertThat(TransitionLogger.sResult, contains("enter: a", "exit: a"));
    }

    @Test
    public void testNotIsActivatedAtTheStart() {
        Controller<String> invertThis = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable.not(invertThis).watch(() -> {
            result.add("enter inverted");
            return () -> result.add("exit inverted");
        });
        assertThat(result, contains("enter inverted"));
    }

    @Test
    public void testNotIsDeactivatedAtTheStartIfSourceIsAlreadyActivated() {
        Controller<String> invertThis = new Controller<>();
        List<String> result = new ArrayList<>();
        invertThis.set("way ahead of you");
        Observable.not(invertThis).watch(() -> {
            result.add("enter inverted");
            return () -> result.add("exit inverted");
        });
        assertThat(result, emptyIterable());
    }

    @Test
    public void testNotExitsWhenSourceIsActivated() {
        Controller<String> invertThis = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable.not(invertThis).watch(() -> {
            result.add("enter inverted");
            return () -> result.add("exit inverted");
        });
        invertThis.set("first");
        assertThat(result, contains("enter inverted", "exit inverted"));
    }

    @Test
    public void testNotReentersWhenSourceIsReset() {
        Controller<String> invertThis = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable.not(invertThis).watch(() -> {
            result.add("enter inverted");
            return () -> result.add("exit inverted");
        });
        invertThis.set("first");
        invertThis.reset();
        assertThat(result, contains("enter inverted", "exit inverted", "enter inverted"));
    }
}
