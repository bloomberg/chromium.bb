# Writing Tests

- _Suites_ contain multiple:
  - _READMEs_
  - _Test Spec Files_. Contains one:
    - _Test Group_. Defines a _test fixture_ and contains multiple:
      - _Tests_. Defines a _test function_ and contains multiple:
        - _Test Cases_, each with the same test function but different _parameters_.

## Suite

A suite of tests.
A single suite has a directory structure, and many _test spec files_
(`.spec.ts` files containing tests) and _READMEs_.
Each member of a suite is identified by its path within the suite.

**Example:** `src/suites/*/`

### README

**Example:** `src/suites/**/README.txt`

Describes (in prose) the contents of a subdirectory in a suite.

**Type:** After a README is loaded, it is stored as a `ReadmeFile`.

## IDs

### Test Spec ID

Uniquely identifies a single test spec file.
Comprised of suite name (`suite`) and test spec file path relative to the suite root (`path`).

**Type:** `TestSpecID`

**Example:** `{ suite: 'cts', path: 'command_buffer/compute/basic' }` corresponds to
`src/suites/cts/command_buffer/compute/basic.spec.ts`.

### Test Case ID

Uniquely identifies a single test case within a test spec.

Comprised of test name (`test`) and the parameters for a case (`params`).
(If `params` is null, there is only one case for the test.)

**Type:** `TestCaseID`

**Example:** `{ test: '', params: { 'value': 1 } }`

## Listing

A listing of the **test spec files** in a suite.

This can be generated only in Node, which has filesystem access (see `src/tools/crawl.ts`).
As part of the build step, a _listing file_ is generated (see `src/tools/gen.ts`) so that the
test files can be discovered by the web runner (since it does not have filesystem access).

**Type:** `TestSuiteListing`

### Listing File

**Example:** `out/suites/*/index.js`

## Test Spec

May be either a _test spec file_ or a _filtered test spec_.
It is identified by a `TestSpecID`.

Always contains one `RunCaseIterable`.

**Type:** `TestSpec`

### Test Spec File

A single `.spec.ts` file. It always contains one _test group_.

**Example:** `src/suites/**/*.spec.ts`

### Filtered Test Spec

A subset of the tests in a single test spec file.
It is created at runtime, via a filter.

It contains one "virtual" test group, which has type `RunCaseIterable`.

## Test Group / Group

A collection of test cases. There is one per test spec file.

**Type:** `TestGroup`

## Test

One test. It has a single _test function_.

It may represent multiple _test cases_, each of which runs the same test function with different **parameters**.

It is created by `TestGroup.test()`.
At creation time, it can be parameterized via `Test.params()`.

**Type:** `Test`

### Test Function

A test function is defined inline inside of a `TestGroup.test()` call.

It receives an instance of the appropriate _test fixture_, through which it produce test results.

**Type:** `TestFn`

## Test Case / Case

A single case of a test. It is identified by a `TestCaseID`: a test name, and its parameters.

**Type:** During test run time, a case is encapsulated as a `RunCase`.

## Parameters / Params

## Test Fixture / Fixture

Test fixtures provide helpers for tests to use.
A new instance of the fixture is created for every run of every test case.

There is always one fixture class for a whole test group (though this may change).

The fixture is also how a test gets access to the _case recorder_,
which allows it to produce test results.

They are also how tests produce results: `.log()`, `.fail()`, etc.

**Type:** `Fixture`

### `UnitTest` Fixture

Provides basic fixture utilities most useful in the `unittests` suite.

### `GPUTest` Fixture

Provides utilities useful in WebGPU CTS tests.

# Running Tests

- _Queries_ contain multiple:
  - _Filters_ (positive or negative).

## Query

A query is a string which denotes a subset of a test suite to run.
It is comprised of a list of filters.
A case is included in the subset if it matches any of the filters.

In the WPT and standalone harnesses, the query is stored in the URL.

Queries are selectively URL-encoded for readability and compatibility with browsers.

**Example:** `?q=unittests:param_helpers:combine/=`
**Example:** `?q=unittests:param_helpers:combine/mixed=`

**Type:** None yet. TODO

### Filter

A filter matches a set of cases in a suite.

Each filter may match one of:

- `S:s` In one suite `S`, all specs whose paths start with `s` (which may be empty).
- `S:s:t` In one spec `S:s`, all tests whose names start with `t` (which may be empty).
- `S:s:t~c` In one test `S:s:t`, all cases whose params are a superset of `c`.
- `S:s:t=c` In one test `S:s:t`, the single case whose params equal `c` (empty string = `{}`).

**Type:** `TestFilter`

### Using filters to split expectations

A set of cases can be split using negative filters. For example, imagine you have one WPT test variant:

- `webgpu/cts.html?q=unittests:param_helpers:`

But one of the cases is failing. To be able to suppress the failing test without losing test coverage, the WPT test variant can be split into two variants:

- `webgpu/cts.html?q=unittests:param_helpers:&not=unittests:param_helpers:combine/mixed:`
- `webgpu/cts.html?q=unittests:param_helpers:combine/mixed:`

This runs the same set of cases, but in two separate page loads.

# Test Results

## Logger

A logger logs the results of a whole test run.

It saves an empty `LiveTestSpecResult` into its results array, then creates a
_test spec recorder_, which records the results for a group into the `LiveTestSpecResult`.

### Test Spec Recorder

Refers to a `LiveTestSpecResult` in the logger, and writes results into it.

It saves an empty `LiveTestCaseResult` into its `LiveTestSpecResult`, then creates a
_test case recorder_, which records the results for a case into the `LiveTestCaseResult`.

### Test Case Recorder

Records the actual results of running a test case (its pass-status, run time, and logs)
into its `LiveTestCaseResult`.

#### Test Case Status

The `status` of a `LiveTestCaseResult` can be one of:

- `'running'`
- `'fail'`
- `'warn'`
- `'pass'`

The "worst" result from running a case is always reported.

## Results Format

The results are returned in JSON format.

They are designed to be easily merged in JavaScript.
(TODO: Write a merge tool.)

(TODO: Update these docs if the format changes.)

```js
{
  "version": "e24c459b46c4815f93f0aed5261e28008d1f2882-dirty",
  "results": [
    // LiveTestSpecResult objects
    {
      "spec": "unittests:basic:",
      "cases": [
        // LiveTestCaseResult objects
        {
          "test": "test/sync",
          "params": null,
          "status": "pass",
          "timems": 145,
          "logs": []
        },
        {
          "test": "test/async",
          "params": null,
          "status": "pass",
          "timems": 26,
          "logs": []
        }
      ]
    },
    {
      "spec": "unittests:test_group:",
      "cases": [
        {
          "test": "invalid test name",
          "params": { "char": "\"" },
          "status": "pass",
          "timems": 66,
          "logs": ["OK: threw"]
        },
        {
          "test": "invalid test name",
          "params": { "char": "`" },
          "status": "pass",
          "timems": 15,
          "logs": ["OK: threw"]
        }
      ]
    }
  ]
}
```
