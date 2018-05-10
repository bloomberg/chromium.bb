# Android Studio

[TOC]

## Usage

Make sure you have followed
[android build instructions](android_build_instructions.md) already.

```shell
build/android/gradle/generate_gradle.py --output-directory out/Debug
```

This creates a project at `out/Debug/gradle`. To create elsewhere:

```shell
build/android/gradle/generate_gradle.py --output-directory out/Debug --project-dir my-project
```

If you are planning to use Android emulators use the
--sdk=AndroidStudioDefault or the --sdk-path option, since adding emulator
images to the project sdk will modify the project sdk, hence causing problems
when you next run gclient sync.

See [android_test_instructions.md](android_test_instructions.md#Using-Emulators)
for more information about building and running emulators.

For first-time Android Studio users:

* Only run the setup wizard if you are planning to use emulators.
    * The wizard will force you to download SDK components that are only needed
      for emulation.
    * To skip it, select "Cancel" when it comes up.

To import the project:

* Use "Import Project", and select the directory containing the generated
  project, by default `out/Debug/gradle`.

If you're asked to use Studio's Android SDK:

* No. (Always use your project's SDK configured by generate_gradle.py)

If you're asked to use Studio's Gradle wrapper:

* Yes.

You need to re-run `generate_gradle.py` whenever `BUILD.gn` files change.

Pass `--canary` or `--beta` to avoid the "A newer version of gradle is
available" notification.

* After regenerating, Android Studio should prompt you to "Sync". If it
  doesn't, try some of the following options:
    * File -&gt; "Sync Project with Gradle Files"
    * Button with two arrows on the right side of the top strip.
    * Help -&gt; Find Action -&gt; "Sync Project with Gradle Files"
    * After `gn clean` you may need to restart Android Studio.
    * File -&gt; "Invalidate Caches / Restart..."

## How It Works

By default, only an `_all` module containing all java apk targets is generated.
If just one apk target is explicitly specified, then a single apk module is
generated.

To see more detailed structure of gn targets, the `--split-projects` flag can
be used. This will generate one module for every gn target in the dependency
graph. This can be very slow when used with `--all` by default.

### Excluded Files

Gradle supports source directories but not source files. However, files in
Chromium are used amongst multiple targets. To accommodate this, the script
detects such targets and creates exclude patterns to exclude files not in the
current target. The editor does not respect these exclude patterns, so the
`_all` pseudo module is added which includes directories from all targets. This
allows imports and refactoring to be across all targets.

### Extracting .srcjars

Most generated .java files in GN are stored as `.srcjars`. Android Studio does
not support them. It is very slow to build all these generated files and they
rarely change. The generator script does not do anything with them by default.
If `--full` is passed then the generator script builds and extracts them all to
`extracted-srcjars/` subdirectories for each target that contains them. This is
the reason that the `_all` pseudo module may contain multiple copies of
generated files.

*** note
** TLDR:** Re-generate project files with `--full` when generated files change (
includes `R.java`) and to remove some red underlines in java files.
***

### Native Files

A new experimental option is now available to enable editing native C/C++ files
with Android Studio. Pass in any number of `--native-target [target name]` flags
in order to try it out. This will require `cmake` and `ndk` packages for your
android SDK. This [crbug](https://crbug.com/840542) tracks adding those to our
`android_tools` repository. For the interim accepting Android Studio's prompts
should work. Example below.

```shell
build/android/gradle/generate_gradle.py --native-target //chrome/android:monochrome
```

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
* Turn off indent notification:
    * Help -&gt; Find Action -&gt; "Show notifications about detected indents"
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

## Status (as of May 10, 2018)

### What works

* Android Studio v3.0-v3.2.
* Java editing.
* Native code editing (experimental).
* Instrumentation tests included as androidTest.
* Symlinks to existing .so files in jniLibs (doesn't generate them).
* Editing resource xml files
* Layout editor (limited functionality).
* Java debugging (see
[here](/docs/android_debugging_instructions.md#Android-Studio)).
* Import resolution and refactoring across java files.
* Correct lint and AndroidManifest when only one target is specified.

### What doesn't work (yet) ([crbug](https://bugs.chromium.org/p/chromium/issues/detail?id=620034))

* Gradle being aware of assets.
* Having the "Make Project" button work correctly.
