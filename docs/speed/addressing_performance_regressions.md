# Addressing Performance Regressions

The bisect bot just picked your CL as the culprit in a performance regression
and assigned a bug to you! What should you do? Read on...

## About our performance tests

The [chromium.perf waterfall](perf_waterfall.md) is a continuous build which
runs performance tests on dozens of devices across Windows, Mac, Linux, and
Android Chrome and WebView. Often, a performance regression only affects a
certain type of hardware or a certain operating system, which may be different
than what you tested locally before landing your CL.

Each test has an owner, named in
[this spreasheet](https://docs.google.com/spreadsheets/d/1xaAo0_SU3iDfGdqDJZX_jRV0QtkufwHUKH3kQKF3YQs/edit#gid=0),
who you can cc on a performance bug if you have questions.

## Understanding the bisect results

The bisect bot spits out a comment on the bug that looks like this:

```
=== BISECT JOB RESULTS ===
Perf regression found with culprit

Suspected Commit
Author : Your Name
Commit : 15092e9195954cbc331cd58e344d0895fe03d0cd
Date : Wed Jun 14 03:09:47 2017
Subject: Your CL Description.

Bisect Details
Configuration: mac_pro_perf_bisect
Benchmark : system_health.common_desktop
Metric : timeToFirstContentfulPaint_avg/load_search/load_search_taobao
Change : 15.25% | 1010.02 -> 1164.04

Revision Result N
chromium@479147 1010.02 +- 1535.41 14 good
chromium@479209 699.332 +- 1282.01 6 good
chromium@479240 383.617 +- 917.038 6 good
chromium@479255 649.186 +- 1896.26 14 good
chromium@479262 788.828 +- 1897.91 14 good
chromium@479268 880.727 +- 2235.29 21 good
chromium@479269 886.511 +- 1150.91 6 good
chromium@479270 1164.04 +- 979.746 14 bad <--

To Run This Test
src/tools/perf/run_benchmark -v --browser=release --output-format=chartjson --upload-results --pageset-repeat=1 --also-run-disabled-tests --story-filter=load.search.taobao system_health.common_desktop
```

There's a lot of information packed in that bug comment! Here's a breakdown:

  * **What regressed exactly?** The comment gives you several details:
    * **The benchmark that regressed**: Under `Bisect Details`, you can see
      `Benchmark :`. In this case, the `system_health.common_desktop`
      benchmark regressed.
    * **What platform did it regress on?** Under `Configuration`, you can find
      some details on the bot that regressed. In this example, it is a Mac Pro
      laptop.
    * **How do I run that locally?** Follow the instructions under
      `To Run This Test`. But be aware that if it regressed on Android and
      you're developing on Windows, you may not be able to reproduce locally.
      (See Debugging regressions below)
    * **What is this testing?** Generally the metric
      (`timeToFirstContentfulPaint_avg`) gives some information. If you're not
      familiar, you can cc the [benchmark owner](https://docs.google.com/spreadsheets/d/1xaAo0_SU3iDfGdqDJZX_jRV0QtkufwHUKH3kQKF3YQs/edit#gid=0)
      to ask for help.
    * **How severe is this regression?** There are different axes on which to
      answer that question:
      * **How much did performance regress?** The bisect bot answers this both
        in relative terms (`Change : 15.25%`) and absolute terms
        (`1010.02 -> 1164.04`). To understand the absolute terms, you'll need
        to look at the units on the performance graphs linked in comment #1
        of the bug (`https://chromeperf.appspot.com/group_report?bug_id=XXX`).
        In this example, the units are milliseconds; the time to load taobao
        regressed from ~1.02 second to 1.16 seconds.
      * **How widespread is the regression?** The graphs linked in comment #1
        of the bug (`https://chromeperf.appspot.com/group_report?bug_id=XXX`)
        will give you an idea how widespread the regression is. The `Bot`
        column shows all the different bots the regression occurred on, and the
        `Test` column shows the metrics it regressed on. Often, the same metric
        is gathered on many different web pages. If you see a long list of
        pages, it's likely that the regression affects most pages; if it's
        short maybe your regression is an edge case.

## Debugging regressions

  * **How do I run the test locally???** Follow the instructions under
    `To Run This Test` in the bisect comment. But be aware that regressions
    are often hardware and/or platform-specific.
  * **What do I do if I don't have the right hardware to test locally?** If
    you don't have a local machine that matches the specs of the hardware that
    regressed, you can run a perf tryjob on the same lab machines that ran the
    bisect that blamed your CL.
    [Here are the instructions for perf tryjobs](perf_trybots.md).
    Drop the `perf_bisect` from the bot name and substitute dashes for
    underscores to get the trybot name (`mac_pro_perf_bisect` -> `mac_pro`
    in the example above).
  * **Can I get a trace?** For most metrics, yes. Here are the steps:
    1. Click on the `All graphs for this bug` link in comment #1. It should
       look like this:
       `https://chromeperf.appspot.com/group_report?bug_id=XXXX`
    2. Select a bot/test combo that looks like what the bisect bot originally
       caught. You might want to look through various regressions for a really
       large increase.
    3. On the graph, click on the exclamation point icon at the regression, and
       a tooltip comes up. There is a "trace" link in the tooltip, click it to
       open a the trace that was recorded during the performance test.
  * **Wait, what's a trace?** See the
    [documentation on tracing](https://www.chromium.org/developers/how-tos/trace-event-profiling-tool)
    to learn how to use traces to debug performance issues.
  * **Are there debugging tips specific to certain benchmarks?**
    * **[Memory](https://chromium.googlesource.com/chromium/src/+/master/docs/memory-infra/memory_benchmarks.md)**
    * **[Android binary size](apk_size_regressions.md)**

## If you don't believe your CL could be the cause

There are some clear reasons to believe the bisect bot made a mistake:

  * Your CL changes a test or some code that isn't compiled on the platform
    that regressed.
  * Your CL is completely unrelated to the metric that regressed.
  * You looked at the numbers the bisect spit out (see example above; the first
    column is the revision, the second column is the value at that revision,
    and the third column is the standard deviation), and:
    * The change attributed to your CL seems well within the noise, or
    * The change at your CL is an improvement (for example, the metric is bytes
      of memory used, and the value goes **down** at your CL) or
    * The change is far smaller that what's reported in the bug summary (for
      example, the bug says there is a 15% memory regression but the bisect
      found that your CL increases memory by 0.77%)

Do the following:

  * Add a comment to the bug explaining why you believe your CL is not the
    cause of the regression.
  * **Unassign yourself from the bug**. This lets our triage process know that
    you are not actively working on the bug.
  * Kick off another bisect. You can do this by:
    1. Click on the `All graphs for this bug` link in comment #1. It should
       look like this:
       `https://chromeperf.appspot.com/group_report?bug_id=XXXX`
    2. Sign in to the dashboard with your chromium.org account in the upper
       right corner.
    3. Select a bot/test combo that looks like what the bisect bot originally
       caught. You might want to look through various regressions for a really
       clear increase.
    4. On the graph, click on the exclamation point icon at the regression, and
       a tooltip comes up. Click the `Bisect` button on the tooltip.


## If you believe the regression is justified

Sometimes you are aware that your CL caused a performance regression, but you
believe the CL should be landed as-is anyway. Chrome's
[core principles](https://www.chromium.org/developers/core-principles) state:

> If you make a change that regresses measured performance, you will be required to fix it or revert.

**It is your responsibility to justify the regression.** You must add a comment
on the bug explaining your justification clearly before WontFix-ing.

Here are some common justification scenarios:

  * **Your change regresses this metric, but is a net positive for performance.**
    There are a few ways to demonstrate that this is true:
    * **Use benchmark results.** If your change has a positive impact, there
      should be clear improvements detected in benchmarks. You can look at all
      the changes (positive and negative) the perf dashboard detected by
      entering the commit position of a change into this url:
      `https://chromeperf.appspot.com/group_report?rev=YOUR_COMMIT_POS_HERE`
      All of these changes are generally changes found on a CL range, and may
      not be attributable to your CL. You can bisect any of these to find if
      your CL caused the improvement, just like you can bisect to find if it
      caused the regression.
    * **Use finch trial results.** There are some types of changes that cannot
      be measured well in benchmarks. If you believe your case falls into this
      category, you can show that end users are not affected via a finch trial.
      See the "End-user metrics" section of
      [How does Chrome measure performance](how_does_chrome_measure_performance.md)
  * **Your change is a critical correctness or security fix.**
    It's true that sometimes something was "fast" because it was implemented
    incorrectly. In this case, a justification should clarify the performance
    cost we are paying for the fix and why it is worth it. Some things to
    include:
    * **What did the benchmark regression cost?** Look at the
      list of regressions in bug comment 1:
      `https://chromeperf.appspot.com/group_report?bug_id=XXXX`
      What is the absolute cost (5MiB RAM? 200ms on page load?)
      How many pages regressed? How many platforms?
    * **What do we gain?** It could be something like:
      * Reduced code complexity
      * Optimal code or UI correctness
      * Additional security
      * Knowledge via an experiment
      * Marketing - something good for users
    * **Is there a more performant way to solve the problem?**
      The [benchmark owner](https://docs.google.com/spreadsheets/d/1xaAo0_SU3iDfGdqDJZX_jRV0QtkufwHUKH3kQKF3YQs/edit#gid=0)
      can generally give you an idea how much work it would take to make a
      similarly-sized performance gain. For example, it might take 1.5
      engineering years to save 3MiB of RAM on Android; could you solve the
      problem in a way that takes less memory than that in less than 1.5 years?
  * **This performance metric is incorrect.** Not all tests are perfect. It's
    possible that your change did not regress performance, and only appears to
    be a problem because the test is measuring incorrectly. If this is the
    case, you must explain clearly what the issue with the test is, and why you
    believe your change is performance neutral. Please include data from traces
    or other performance tools to clarify your claim.

**In all cases,** make sure to cc the [benchmark owner](https://docs.google.com/spreadsheets/d/1xaAo0_SU3iDfGdqDJZX_jRV0QtkufwHUKH3kQKF3YQs/edit#gid=0)
when writing a justification and WontFix-ing a bug. If you cannot come to an
agreement with the benchmark owner, you can escalate to benhenry@chromium.org,
the owner of speed releasing.