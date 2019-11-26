# Trybot: android-binary-size

[TOC]

## About

The android-binary-size trybot exists for three reasons:
1. To measure and make developers aware of the binary size impact of commits.
2. To perform checks that require comparing builds with & without patch.
3. To provide bot coverage for building with `is_official_build=true`.

## Measurements and Analysis

The bot provides analysis using:
* [resource_sizes.py]: The delta in metrics are reported. Most of these are
  described in [//docs/speed/binary_size/metrics.md][metrics].
* [SuperSize]: Provides visual and textual binary size breakdowns.

[resource_sizes.py]: /build/android/resource_sizes.py
[metrics]: /docs/speed/binary_size/metrics.md
[SuperSize]: /tools/binary_size/README.md

## Checks:

### Binary Size Increase

- **What:** Checks that [normalized apk size] increases by no more than 16kb.
- **Why:** While we hope that binary size impact of all commits are looked at
  to ensure they make sense, this check is to ensure they are looked at for
  larger than average commits.

[normalized apk size]: /docs/speed/binary_size/metrics.md#normalized-apk-size

#### What to do if the Check Fails?

- Look at the provided symbol diffs to understand where the size is coming from.
- See if any of the generic [optimization advice] is applicable.
- If you are writing a new feature or including a new library you might want to
  think about skipping the android platform and to restrict this new
  feature/library to desktop platforms that might care less about binary size.
- If reduction is not practical, add a rationale for the increase to the commit
  description. It should include:
    - A list of any optimizations that you attempted (if applicable)
    - If you think that there might not be a consensus that the code your adding
      is worth the added file size, then add why you think it is.
        - To get a feeling for how large existing features are, refer to
          [milestone size breakdowns].

- Add a footer to the commit description along the lines of:
    - `Binary-Size: Size increase is unavoidable (see above).`
    - `Binary-Size: Increase is temporary.`

[optimization advice]: /docs/speed/binary_size/optimization_advice.md
[milestone size breakdowns]: https://storage.googleapis.com/chrome-supersize/index.html



### Dex Method Count

- **What:** Checks that the number of Java methods after optimization does not
  increase by more than 50.
- **Why:** Ensures that large changes to this metric are scrutinized.

#### What to do if the Check Fails?

- Look at the bot's "Dex Class and Method Diff" output to see which classes and
  methods survived optimization.
- See if any of [Java Optimization] tips are applicable.
- If the increase is from a new dependency, ensure that there is no existing
  library that provides similar functionality.
- If reduction is not practical, add a rationale for the increase to the commit
  description. It should include:
    - A list of any optimizations that you attempted (if applicable)
    - If you think that there might not be a consensus that the code your adding
      is worth the added file size, then add why you think it is.
        - To get a feeling for how large existing features are, open the latest
          [milestone size breakdown] and select "Method Count Mode".
- Add a footer to the commit description along the lines of:
    - `Binary-Size: Added a new library.`
    - `Binary-Size: Enables a large feature that was previously flagged.`

[Java Optimization]: /docs/speed/binary_size/optimization_advice.md#Optimizating-Java-Code

### Mutable Constants

- **What**: Checks that all variables named `kVariableName` are in read-only
  sections of the binary (either `.rodata` or `.data.rel.do`).
- **Why**: Guards against accidentally missing a `const` keyword. Non-const
  variables have a larger memory footprint than const ones.
- For more context see [https://crbug.com/747064](https://crbug.com/747064).

#### What to do if the Check Fails?

- Make the symbol read-only (usually by adding "const").
- If you can't make it const, then rename it.
- To check what section a symbol is in for a local build:
  ```sh
  ninja -C out/Release obj/.../your_file.o
  third_party/llvm-build/Release+Asserts/bin/llvm-nm out/Release/.../your_file.o --format=darwin
  ```
  - Only `format=darwin` shows the difference between `.data` and `.data.rel.ro`.
  - You need to use llvm's `nm` only when thin-lto is enabled
    (when `is_official_build=true`).

Here's the most common example:
```c++
const char * kMyVar = "...";  // A *mutable* pointer to a const char (bad).
const char * const kMyVar = "..."; // A const pointer to a const char (good).
constexpr char * kMyVar = "..."; // A const pointer to a const char (good).
const char kMyVar[] = "..."; // A const char array (good).
```

For more information on when to use `const char *` vs `const char[]`, see
[//docs/native_relocations.md](/docs/native_relocations.md).

### Added Symbols named “ForTest”

- **What:** This checks that we don't have symbols with “ForTest” in their name
  in an optimized release binary.
- **Why:** To prevent shipping unused test-only code to end-users.

#### What to do if the Check Fails?

- Make sure your ForTest methods are not called in non-test code.
- Unfortunately, clang is unable to remove unused virtual methods, so try and
  make sure your ForTest methods are not virtual.

### Uncompressed Pak Entry

- **What:** Checks that `.pak` file entries that are not translatable strings
  and are stored compressed. Limit currently set to 1KB.
- **Why:** Compression makes things smaller and there is normally no reason to
  leaving resources uncompressed.

#### What to do if the Check Fails?

- Add `compress="gzip"` to the `.grd` entry for the resource.

### Expectation Failures

- **What & Why:** Learn about these expectation files [here][expectation files].

[expectation files]: /chrome/android/java/README.md

#### What to do if the Check Fails?

- The output of the failing step contains the command to run to update the
  relevant expectation file. Run this command to update the expectation files.

### If All Else Fails

- For help, email [binary-size@chromium.org]. Hearing about your issues helps us
  to improve the tools!
- Not all checks are perfect and sometimes you want to overrule the trybot (for
  example if you did your best and are unable to reduce binary size any
  further).
- Adding a “Binary-Size: $ANY\_TEXT\_HERE” footer to your cl (next to “Bug:”)
  will bypass the bot assertions.
    - Most commits that trigger the warnings will also result in Telemetry
      alerts and be reviewed by a binary size sheriff. Failing to write an
      adequate justification may lead to the binary size sheriff filing a bug
      against you to improve your cl.

[binary-size@chromium.org]: https://groups.google.com/a/chromium.org/forum/#!forum/binary-size

## Bot Links Provided by the Last Step

### Size Assertion Results

- Shows the list of checks that ran grouped by passing and failing checks.
- Read this to know which checks failed the tryjob.

### Supersize text diff

- This is the text diff produced by the supersize tool.
- It lists all changed symbols and for each one, which section it lives in,
  which source file it came from as well as what is its size before, after and
  the delta for your cl.
- It also contains a histogram of symbol size deltas.
- You can use this to find which symbols grew and where the binary size impact
  of your cl comes from.

### Supersize html diff

- Visual representation of the text diff above.
- It shows size deltas per file and directory
- It allows you to filter symbols by type/section/size/etc.

## Code Locations

- [Link to recipe](https://cs.chromium.org/chromium/build/scripts/slave/recipes/binary_size_trybot.py)
- [Link to src-side checks](/tools/binary_size/trybot_commit_size_checker.py)
