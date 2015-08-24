# Introduction

**WARNING:** This is all very alpha. Proceed at your own risk. The Mac instructions are very out of date -- IWYU currently isn't generally usable, so we stopped looking at it for chromium.

See [include what you use page](http://code.google.com/p/include-what-you-use/) for background about what it is and why it is important.

This page describes running IWYU for Chromium.

# Linux/Blink

## Running IWYU

These instructions have a slightly awkward workflow. Ideally we should use something like `CXX=include-what-you-use GYP_DEFINES="clang=1" gclient runhooks; ninja -C out/Debug webkit -k 10000` if someone can get it working.

  * Install include-what-you-use (see [here](https://code.google.com/p/include-what-you-use/wiki/InstructionsForUsers)). Make sure to use --enable-optimized=YES when building otherwise IWYU will be very slow.
  * Get the compilation commands from ninja (using g++), and derive include-what-you-use invocations from it
```
$ cd /path/to/chromium/src
$ ninja -C out/Debug content_shell -v > ninjalog.txt
$ sed '/obj\/third_party\/WebKit\/Source/!d; s/^\[[0-9\/]*\] //; /^g++/!d; s/^g++/include-what-you-use -Wno-c++11-extensions/; s/-fno-ident//' ninjalog.txt > commands.txt
```
  * Run the IWYU commands. We do this in parallel for speed. Merge the output and remove any complaints that the compiler has.
```
$ cd out/Debug
$ for i in {1..32}; do (sed -ne "$i~32p" ../../commands.txt | xargs -n 1 -L 1 -d '\n' bash -c > iwyu_$i.txt 2>&1) & done
$ cat iwyu_{1..32}.txt | sed '/In file included from/d;/\(note\|warning\|error\):/{:a;N;/should add/!b a;s/.*\n//}' > iwyu.txt
$ rm iwyu_{1..32}.txt
```
  * The output in iwyu.txt has all the suggested changes

# Mac

## Setup

  1. Checkout and build IWYU (This will also check out and build clang. See [Clang page](http://code.google.com/p/chromium/wiki/Clang) for details.)
```
$ cd /path/to/src/
$ tools/clang/scripts/update_iwyu.sh
```
  1. Ensure "Continue building after errors" is enabled in the Xcode Preferences UI.

## Chromium

  1. Build Chromium. Be sure to substitute in the correct absolute path for `/path/to/src/`.
```
$ GYP_DEFINES='clang=1' gclient runhooks
$ cd chrome
$ xcodebuild -configuration Release -target chrome OBJROOT=/path/to/src/clang/obj DSTROOT=/path/to/src/clang SYMROOT=/path/to/src/clang CC=/path/to/src/third_party/llvm-build/Release+Asserts/bin/clang++
```
  1. Run IWYU. Be sure to substitute in the correct absolute path for `/path/to/src/`.
```
$ xcodebuild -configuration Release -target chrome OBJROOT=/path/to/src/clang/obj DSTROOT=/path/to/src/clang SYMROOT=/path/to/src/clang CC=/path/to/src/third_party/llvm-build/Release+Asserts/bin/include-what-you-use
```

## WebKit

  1. Build TestShell. Be sure to substitute in the correct absolute path for `/path/to/src/`.
```
$ GYP_DEFINES='clang=1' gclient runhooks
$ cd webkit
$ xcodebuild -configuration Release -target test_shell OBJROOT=/path/to/src/clang/obj DSTROOT=/path/to/src/clang SYMROOT=/path/to/src/clang CC=/path/to/src/third_party/llvm-build/Release+Asserts/bin/clang++
```
  1. Run IWYU. Be sure to substitute in the correct absolute path for `/path/to/src/`.
```
$ xcodebuild -configuration Release -target test_shell OBJROOT=/path/to/src/clang/obj DSTROOT=/path/to/src/clang SYMROOT=/path/to/src/clang CC=/work/chromium/src/third_party/llvm-build/Release+Asserts/bin/include-what-you-use
```

# Bragging

You can run `tools/include_tracer.py` to get header file sizes before and after running iwyu. You can then include stats like "This reduces the size of foo.h from 2MB to 80kB" in your CL descriptions.

# Known Issues

We are a long way off from being able to accept the results of IWYU for Chromium/Blink. However, even in its current state it can be a useful tool for finding forward declaration opportunities and unused includes.

Using IWYU with Blink has several issues:
  * Lack of understanding on Blink style, e.g. config.h, wtf/MathExtras.h, wtf/Forward.h, wtf/Threading.h
  * "using" declarations (most of WTF) makes IWYU not suggest forward declarations
  * Functions defined inline in a different location to the declaration are dropped, e.g. Document::renderView in RenderView.h and Node::renderStyle in NodeRenderStyle.h
  * typedefs can cause unwanted dependencies, e.g. typedef int ExceptionCode in Document.h
  * .cpp files don't always correspond directly to .h files, e.g. Foo.h can be implemented in e.g. chromium/FooChromium.cpp
  * g++/clang/iwyu seems fine with using forward declarations for PassRefPtr types in some circumstances, which MSVC doesn't.