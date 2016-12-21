// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util.parameter;

import android.app.Activity;
import android.support.test.filters.SmallTest;
import android.test.MoreAsserts;

import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.parameter.parameters.MethodParameter;

/**
 * Tester class for the {@link ParameterizedTest} annotation and the {@link ParameterizedTest.Set}
 * annotation.
 *
 * Also, tests the {@link MethodParameter} parameter.
 */
public class ParameterizedTestAnnotationTest extends
        BaseActivityInstrumentationTestCase<Activity> {
    public ParameterizedTestAnnotationTest() {
        super(Activity.class);
    }

    @SmallTest
    public void testNoParameterizedTestAnnotation() {
        assertFalse("This is a parameterized test when it should not be.", getParameterReader()
                .isParameterizedTest());
        assertNull("someParameter should not exist.", getParameterReader()
                .getParameter("someParameter"));
        assertNull("someParameterArgument should not exist.", getParameterReader()
                .getParameterArgument("someParameter", "someParameterArgument"));
    }

    @SmallTest
    @ParameterizedTest()
    public void testEmptyParameterizedTestAnnotation() {
        assertTrue("This is not a parameterized test.", getParameterReader()
                .isParameterizedTest());
        assertNull("someParameter should not exist.", getParameterReader()
                .getParameter("someParameter"));
        assertNull("someParameterArgument should not exist.", getParameterReader()
                .getParameterArgument("someParameter", "someParameterArgument"));
    }

    @SmallTest
    @ParameterizedTest(parameters = {})
    public void testParameterizedTestWithEmptyParameters() {
        assertTrue("This is not a parameterized test.", getParameterReader()
                .isParameterizedTest());
        assertNull("someParameter should not exist.", getParameterReader()
                .getParameter("someParameter"));
        assertNull("someParameterArgument should not exist.", getParameterReader()
                .getParameterArgument("someParameter", "someParameterArgument"));
    }

    @SmallTest
    @ParameterizedTest(parameters = {})
    public void testParameterDoesNotExist() {
        Parameter parameter = getParameterReader().getParameter(MethodParameter.PARAMETER_TAG);
        assertNull("method-parameter should not exist.", parameter);
    }

    @SmallTest
    @ParameterizedTest(parameters = {@Parameter(tag = MethodParameter.PARAMETER_TAG)})
    public void testGetParameter() {
        String expected = "method-parameter";
        String actual = getParameterReader().getParameter(MethodParameter.PARAMETER_TAG).tag();
        assertEquals("Parameter tag did not match expected.", expected, actual);
    }

    @SmallTest
    @ParameterizedTest(parameters = {@Parameter(tag = MethodParameter.PARAMETER_TAG)})
    public void testParameterArgumentDoesNotExist() {
        Parameter.Argument actual = getArgument("arg");
        assertNull("arg should not exist.", actual);
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {@Parameter.Argument(name = "string", stringVar = "value")})})
    public void testMethodParametersWithOneStringValue() {
        String expected = "value";
        String actual = getArgument("string").stringVar();
        assertEquals(mismatchMessage("string"), expected, actual);
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {@Parameter.Argument(name = "int", intVar = 0)})})
    public void testMethodParametersWithOneIntValue() {
        int expected = 0;
        int actual = getArgument("int").intVar();
        assertEquals(mismatchMessage("int"), expected, actual);
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {
                            @Parameter.Argument(name = "intArray", intArray = {5, 10, -6, 0, -1})})
            })
    public void testMethodParametersWithOneIntArrayValue() {
        int[] expected = new int[] {5, 10, -6, 0, -1};
        int[] actual = getArgument("intArray").intArray();
        assertEquals("Expected length and actual length are different.", expected.length,
                actual.length);
        MoreAsserts.assertEquals(mismatchMessage("intArray"), expected, actual);
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {@Parameter.Argument(name = "stringArray", stringArray = {
                            "apple", "banana", "orange", "melon", "lemon"})})})
    public void testMethodParametersWithOneStringArrayValue() {
        String[] expected = new String[] {"apple", "banana", "orange", "melon", "lemon"};
        String[] actual = getArgument("stringArray").stringArray();
        assertEquals("Expected length and actual length are different.", expected.length,
                actual.length);
        MoreAsserts.assertEquals(mismatchMessage("stringArary"), expected, actual);
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {
                            @Parameter.Argument(name = "string1", stringVar = "has vowel"),
                            @Parameter.Argument(name = "string2", stringVar = "hs vwl"),
                            @Parameter.Argument(name = "stringArray1", stringArray = {
                                    "apple", "banana", "orange", "melon", "lemon"}),
                            @Parameter.Argument(name = "stringArray2", stringArray = {
                                    "ppl", "bnn", "mln", "lmn"}),
                            @Parameter.Argument(name = "int1", intVar = 2),
                            @Parameter.Argument(name = "int2", intVar = 10),
                            @Parameter.Argument(name = "intArray1", intArray = {
                                    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37}),
                            @Parameter.Argument(name = "intArray2", intArray = {4})})})
    public void testMethodParametersWithMultipleArguments1() {
        String stringVar = getArgument("string1").stringVar();
        assertEquals(mismatchMessage("string1"), "has vowel", stringVar);

        stringVar = getArgument("string2").stringVar();
        assertEquals(mismatchMessage("string2"), "hs vwl", stringVar);

        int intVar = getArgument("int1").intVar();
        assertEquals(mismatchMessage("int1"), 2, intVar);

        intVar = getArgument("int2").intVar();
        assertEquals(mismatchMessage("int2"), 10, intVar);

        String[] stringArray = getArgument("stringArray1").stringArray();
        String[] expectedStringArray = new String[] {"apple", "banana", "orange", "melon", "lemon"};
        MoreAsserts.assertEquals("stringArray1 did not match", expectedStringArray, stringArray);

        stringArray = getArgument("stringArray2").stringArray();
        expectedStringArray = new String[] {"ppl", "bnn", "mln", "lmn"};
        MoreAsserts.assertEquals("stringArray2 did not match", expectedStringArray, stringArray);

        int[] intArray = getArgument("intArray1").intArray();
        int[] expectedIntArray = new int[] {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
        MoreAsserts.assertEquals("intArray1 did not match", expectedIntArray, intArray);

        intArray = getArgument("intArray2").intArray();
        expectedIntArray = new int[] {4};
        MoreAsserts.assertEquals("intArray2 did not match", expectedIntArray, intArray);
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(
                    tag = MethodParameter.PARAMETER_TAG,
                    arguments = {
                            @Parameter.Argument(name = "string1", stringVar = "testvalue"),
                            @Parameter.Argument(name = "string2", stringVar = "blahblah"),
                            @Parameter.Argument(name = "int1", intVar = 4),
                            @Parameter.Argument(name = "int2", intVar = 0)})})
    public void testMethodParametersWithMultipleArguments2() {
        assertEquals("bar variable should equals \"testvalue\"", "testvalue",
                getArgument("string1").stringVar());
        assertEquals("foo variable should equals \"blahblah\"", "blahblah",
                getArgument("string2").stringVar());
        assertEquals("intArg1 variable should equals 4", 4,
                getArgument("int1").intVar());
        assertEquals("intArg2 variable should equals 0", 0,
                getArgument("int2").intVar());
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {@Parameter.Argument(name = "string", stringVar = "t_val")})})
    @ParameterizedTest.Set(tests = {
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "string", stringVar = "s_val")})})})
    public void testParameterizedSetOverridesParameterizedTest() {
        String expected = "s_val";
        String actual = getArgument("string").stringVar();
        assertEquals("Expected the value set via @ParameterizedTest.Set", expected, actual);
    }

    @SmallTest
    @ParameterizedTest.Set(tests = {
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "string1", stringVar = "testvalue"),
                                    @Parameter.Argument(name = "string2", stringVar = "blahblah"),
                                    @Parameter.Argument(name = "int1", intVar = 4),
                                    @Parameter.Argument(name = "int2", intVar = 0)})})})
    public void testParameterArgumentsWithParameterSetOfOneTest() {
        assertEquals("bar variable should equals \"testvalue\"", "testvalue",
                getArgument("string1").stringVar());
        assertEquals("foo variable should equals \"blahblah\"", "blahblah",
                getArgument("string2").stringVar());
        assertEquals("intArg1 variable should equals 4", 4,
                getArgument("int1").intVar());
        assertEquals("intArg2 variable should equals 0", 0,
                getArgument("int2").intVar());
    }

    /**
     *  These next three tests all accomplish the same task but in different ways.
     *
     *  testParameterArgumentsWithParameterSetOfMoreThanOneTest tests fib() by having a set for
     *  each possible input. While this is fine, it is rather verbose.
     *
     *  Continued @ testSingleTestParameterArgumentsWithParameterSet
     */
    @SmallTest
    @ParameterizedTest.Set(tests = {
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 1),
                                    @Parameter.Argument(name = "output", intVar = 0)})}),
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 2),
                                    @Parameter.Argument(name = "output", intVar = 1)})}),
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 3),
                                    @Parameter.Argument(name = "output", intVar = 1)})}),
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 4),
                                    @Parameter.Argument(name = "output", intVar = 2)})}),
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 5),
                                    @Parameter.Argument(name = "output", intVar = 3)})}),
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 6),
                                    @Parameter.Argument(name = "output", intVar = 5)})}),
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 7),
                                    @Parameter.Argument(name = "output", intVar = 8)})}),
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 8),
                                    @Parameter.Argument(name = "output", intVar = 13)})}),
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 9),
                                    @Parameter.Argument(name = "output", intVar = 21)})}),
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input", intVar = 10),
                                    @Parameter.Argument(name = "output", intVar = 34)})})})
    public void testParameterArgumentsWithParameterSetOfMoreThanOneTest() {
        int input = getArgument("input").intVar();
        int expected = getArgument("output").intVar();
        int actual = fib(input);
        assertEquals("Output should be the fibonacci number at index input.", expected, actual);
    }

    /**
     * This is better than the implementation of
     * testParameterArgumentsWithParameterSetOfMoreThanOneTest. It reduces the number of
     * {@link ParameterizedTest} annotations by using an intArray instead of an intVar.
     * Computationally, this will be faster too because it only has to set up the parameters once
     * for the single ParameterizedTest.
     *
     * Continued @ testSingleTestParameterArgumentsWithParameterSet
     */
    @SmallTest
    @ParameterizedTest.Set(tests = {
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "input",
                                            intArray = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}),
                                    @Parameter.Argument(name = "expected",
                                            intArray = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34})})})})
    public void testSingleTestParameterArgumentsWithParameterSet() {
        int[] input = getArgument("input").intArray();
        int[] expected = getArgument("expected").intArray();
        int[] actual = new int[input.length];
        for (int i = 0; i < input.length; i++) {
            actual[i] = fib(input[i]);
        }
        MoreAsserts.assertEquals("Output should be the fibonacci number at each index input.",
                expected, actual);
    }

    /**
     * This is the best implementation to test fibonacci.
     *
     * It's computationally the same speed at testSingleTestParameterArgumentsWithParameterSet. But
     * we don't need to actually have a ParameterizedTestSet.
     */
    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(
                    tag = MethodParameter.PARAMETER_TAG,
                    arguments = {
                            @Parameter.Argument(name = "input",
                                    intArray = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}),
                            @Parameter.Argument(name = "expected",
                                    intArray = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34})})})
    public void testSingleTestParameterArgumentsWithoutParameterSet() {
        int[] input = getArgument("input").intArray();
        int[] expected = getArgument("expected").intArray();
        int[] actual = new int[input.length];
        for (int i = 0; i < input.length; i++) {
            actual[i] = fib(input[i]);
        }
        MoreAsserts.assertEquals("Output should be the fibonacci number at each index input.",
                expected, actual);
    }

    private static String mismatchMessage(String name) {
        return String.format("The ParameterArgument %s does not match expected value.", name);
    }

    private static int fib(int input) {
        if (input <= 0) {
            throw new IllegalArgumentException("Input must be greater than 0.");
        }
        if (input == 1) {
            return 0;
        }
        if (input == 2) {
            return 1;
        }
        int[] temp = new int[input];
        temp[0] = 0;
        temp[1] = 1;
        for (int i = 2; i < input; i++) {
            temp[i] = temp[i - 1] + temp[i - 2];
        }
        return temp[input - 1];
    }

    private Parameter.Argument getArgument(String name) {
        return getParameterReader().getParameterArgument(MethodParameter.PARAMETER_TAG, name);
    }
}
