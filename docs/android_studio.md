# Android Studio

[TOC]

## Usage

Make sure you have followed
[android build instructions](android_build_instructions.md) already.

```shell
build/android/gradle/generate_gradle.py [--canary]  # Use --canary for Android Studio 3.0 beta
```

This creates a project at `out/Debug/gradle`. To create elsewhere:

```shell
build/android/gradle/generate_gradle.py --output-directory out/My-Out-Dir --project-dir my-project
```

By default, common targets are generated. To add more targets to generate
projects for:

```shell
build/android/gradle/generate_gradle.py --extra-target //chrome/android:chrome_public_apk
```

For first-time Android Studio users:

* Avoid running the setup wizard.
    * The wizard will force you to download unwanted SDK components to
      `//third_party/android_tools`.
    * To skip it, select "Cancel" when it comes up.

To import the project:

* Use "Import Project", and select the directory containing the generated
  project, by default `out/Debug/gradle`.

If you're asked to use Studio's Android SDK:

* No.

If you're asked to use Studio's Gradle wrapper:

* Yes.

You need to re-run `generate_gradle.py` whenever `BUILD.gn` files change.

* After regenerating, Android Studio should prompt you to "Sync". If it
  doesn't, use:
    * Button with two arrows on the right side of the top strip.
    * Help -&gt; Find Action -&gt; "Sync Project with Gradle Files"
    * After `gn clean` you may need to restart Android Studio.

## How It Works

By default, only a single module is generated. If more than one apk target is
specified, then an `_all` module is generated. Otherwise a single apk module is
generated. Since instrumentation tests are combined with their `apk_under_test`
target, they count as one module together.

To see more detailed structure of gn targets, the `--split-projects` flag can
be used. This will generate one module for every gn target in the dependency
graph.

### Excluded Files

Gradle supports source directories but not source files. However, files in
Chromium are used amongst multiple targets. To accommodate this, the script
detects such targets and creates exclude patterns to exclude files not in the
current target. The editor does not respect these exclude patterns, so a `_all`
pseudo module is added which includes directories from all targets. This allows
imports and refactorings to be searched across all targets.

### Extracting .srcjars

Most generated .java files in GN are stored as `.srcjars`. Android Studio does
not support them, and so the generator script builds and extracts them all to
`extracted-srcjars/` subdirectories for each target that contains them. This is
the reason that the `_all` pseudo module may contain multiple copies of
generated files.

*** note
** TLDR:** Always re-generate project files when `.srcjars` change (this
includes `R.java`).
***

## Android Studio Tips

* Using the Java debugger is documented at [android_debugging_instructions.md#android-studio](android_debugging_instructions.md#android-studio).
* Configuration instructions can be found
  [here](http://tools.android.com/tech-docs/configuration). One suggestions:
    * Launch it with more RAM:
      `STUDIO_VM_OPTIONS=-Xmx2048m /opt/android-studio-stable/bin/studio-launcher.sh`
* If you ever need to reset it: `rm -r ~/.AndroidStudio*/`
* Import Chromium-specific style and inspections settings:
    * Help -&gt; Find Action -&gt; "Code Style" (settings) -&gt; Java -&gt;
      Scheme -&gt; Import Scheme
        * Select `tools/android/android_studio/ChromiumStyle.xml` -&gt; OK
    * Help -&gt; Find Action -&gt; "Inspections" (settings) -&gt;
      Profile -&gt; Import profile
        * Select `tools/android/android_studio/ChromiumInspections.xml` -&gt; OK
* Turn on automatic import:
    * Help -&gt; Find Action -&gt; "Auto Import"
        * Tick all the boxes under "Java" and change the dropdown to "All".
* Turn on documentation on mouse hover:
    * Help -&gt; Find Action -&gt; "Show quick documentation on mouse move"
* Turn on line numbers:
    * Help -&gt; Find Action -&gt; "Show line numbers"
* Format changed files (Useful for changes made by running code inspection):
    * Set up version control
        * File -&gt; Settings -&gt; Version Control
        * Add src directories
    * Commit changes and reformat
        * Help -&gt; Find Action -&gt; "Commit Changes"
        * Check "Reformat code" & "Optimize imports" and commit

### Useful Shortcuts

* `Shift - Shift`: Search to open file or perform IDE action
* `Ctrl + N`: Jump to class
* `Ctrl + Shift + T`: Jump to test
* `Ctrl + Shift + N`: Jump to file
* `Ctrl + F12`: Jump to method
* `Ctrl + G`: Jump to line
* `Shift + F6`: Rename variable
* `Ctrl + Alt + O`: Organize imports
* `Alt + Enter`: Quick Fix (use on underlined errors)
* `F2`: Find next error

### Building from the Command Line

Gradle builds can be done from the command-line after importing the project
into Android Studio (importing into the IDE causes the Gradle wrapper to be
added). This wrapper can also be used to invoke gradle commands.

    cd $GRADLE_PROJECT_DIR && bash gradlew

The resulting artifacts are not terribly useful. They are missing assets,
resources, native libraries, etc.

* Use a
  [gradle daemon](https://docs.gradle.org/2.14.1/userguide/gradle_daemon.html)
  to speed up builds using the gradlew script:
    * Add the line `org.gradle.daemon=true` to `~/.gradle/gradle.properties`,
      creating it if necessary.

## Status (as of Oct, 2017)

### What works

* Android Studio v2.3, and v3.0 beta with `--canary` flag.
* Java editing and gradle compile.
* Instrumentation tests included as androidTest.
* Symlinks to existing .so files in jniLibs (doesn't generate them).
* Editing resource xml files.
* Java debugging (see
[here](/docs/android_debugging_instructions.md#Android-Studio)).
* Import resolution and refactoring across all modules.
* Correct lint and AndroidManifest when only one target is specified.

### What doesn't work (yet) ([crbug](https://bugs.chromium.org/p/chromium/issues/detail?id=620034))

* Gradle being aware of assets.
* Layout editor.
* Native code editing.
* Having the "Make Project" button work correctly.
