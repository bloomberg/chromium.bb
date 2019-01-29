# Code Coverage in Gerrit

**Have you ever wanted to know if your CL has enough code coverage?** You can
see uncovered lines in Gerrit, which can help you figure out if your CL or the
CL you're reviewing needs more/better tests.

To generate code coverage data for your CL, **manually** trigger the
experimental coverage CQ builder: *linux-coverage-rel*:

![choose_tryjobs] ![linux_coverage_rel]

Once the build finishes and code coverage data are processed successfully, look
at the right column of the side by side diff view to see which lines are **not**
covered:

![uncovered_lines]

## Contacts

### Reporting problems
For any breakage report and feature requests, please [file a bug].

### Mailing list
For questions and general discussions, please join [code-coverage group].

## FAQ
### Why is the coverage CQ builder needs to be triggered manually?

We're still evaluating the stability of the code coverage pipeline, and once
it's proven to be stable, the experimental builder will be merged into one of
the existing CQ builders, and at that time, the feature will be available by
default.

### Why is coverage not shown even though the try job finished successfully?

There are several possible reasons:
* All the added/modified lines are covered.
* A particular source file/test may not be available on a particular project or
platform. As of now, only `chromium/src` project and `Linux` platform is
supported.
* There is a bug in the pipeline. Please [file a bug] to report the breakage.

### How does it work?

Please refer to [code_coverage.md] for how code coverage works in Chromium in
general, and specifically, for per-CL coverage in Gerrit, the
[clang_code_coverage_wrapper] is used to compile and instrument ONLY the source
files that are affected by the CL and a [chromium-coverage Gerrit plugin] is
used to annotate uncovered lines in Gerrit.


[choose_tryjobs]: images/code_coverage_choose_tryjobs.png
[linux_coverage_rel]: images/code_coverage_linux_coverage_rel.png
[uncovered_lines]: images/code_coverage_uncovered_lines.png
[file a bug]: https://bugs.chromium.org/p/chromium/issues/entry?components=Tools%3ECodeCoverage
[code-coverage group]: https://groups.google.com/a/chromium.org/forum/#!forum/code-coverage
[code_coverage.md]: code_coverage.md
[clang_code_coverage_wrapper]: clang_code_coverage_wrapper.md
[chromium-coverage Gerrit plugin]: https://chromium.googlesource.com/infra/gerrit-plugins/code-coverage/
