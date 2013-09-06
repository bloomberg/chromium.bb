.. _devcycle-building:

########
Building
########

.. contents:: Table Of Contents
  :local:
  :backlinks: none
  :depth: 2


Introduction
============

This document describes how to build Native Client modules. It is intended for
developers who have experience writing, compiling, and linking C and C++ code.
If you haven't read the Native Client :doc:`Technical Overview
<../../overview>` and :doc:`Tutorial <../tutorial>`, we recommend starting
with those.

Target architectures
--------------------

Native Client modules are written in C or C++ and compiled into executable
files ending in a .nexe extension using one of the toolchains in the Native
Client SDK. Chrome can load .nexe files embedded in web pages and execute the
.nexe files as part of a web application.

As explained in the Technical Overview, Native Client modules are
operating-system-independent, not processor-independent. Therefore, you must
compile separate versions of your Native Client module for different processors
on end-user machines, such as x86-32, x86-64, and ARM. The list below shows
which .nexe modules run on which target architectures:

**x86 32-bit .nexe modules run on**:

* Windows 32-bit
* Mac
* Linux 32-bit
* Linux 64-bit (but only with 32-bit Chrome)

**x86 64-bit .nexe modules run on**:

* Windows 64-bit
* Linux 64-bit (but only with 64-bit Chrome).

**ARM .nexe modules run on**:

* ARM devices

In general, you must compile a module for the 32-bit and 64-bit x86 target
architectures (we also strongly recommend compiling modules for the ARM
architecture), and create a :ref:`manifest file <application_files>` that
specifies which version of the module to load based on the end-user's
architecture. The SDK includes a script, ``create_nmf.py`` (in the ``tools/``
directory) to generate manifest files. For examples of how to compile modules
for multiple target architectures and how to generate manifest files, see the
Makefiles included with the SDK examples.

C libraries
-----------

The Native Client SDK comes with two C libraries: `newlib
<http://sourceware.org/newlib/>`_ and `glibc
<http://www.gnu.org/software/libc/>`_. See :doc:`Dynamic Linking & Loading with
glibc <dynamic-loading>` for information about these libraries, including
factors to help you decide which to use.

SDK toolchains
--------------

The Native Client SDK includes multiple toolchains, differentiated by target
architectures and C libraries. The toolchains are located in directories named
``toolchain/<platform>_<architecture>_<library>``, where:

* *<platform>* is the platform of your development machine (win, mac, or linux)
* *<architecture>* is your target architecture (x86 or arm)
* *<library>* is the C library you are compiling with (newlib or glibc)

The compilers, linkers, and other tools are located in the bin/ subdirectory in
each toolchain. For example, the tools in the Windows SDK for the x86 target
architecture using the newlib library are in ``toolchain/win_x86_newlib/bin``.

.. Note::
  :class: note

  The SDK toolchains descend from the ``toolchain/`` directory. The SDK also
  has a ``tools/`` directory; this directory contains utilities that are not
  properly part of the toolchains but that you may find helpful in building and
  testing your application (e.g., the ``create_nmf.py`` script, which you can
  use to create a manifest file).

SDK toolchains versus your hosted toolchain
-------------------------------------------

To build .nexe files, you must use one of the Native Client toolchains included
in the SDK. The SDK toolchains use a variety of techniques to ensure that your
``.nexe`` files comply with the security constraints of the Native Client
sandbox.

During development, you have another choice: You can build modules using a
standard GNU toolchain, such as the hosted toolchain on your development
machine. The benefit of using a standard GNU toolchain is that you can develop
modules in your favorite IDE and use your favorite debugging and profiling
tools. The drawback is that modules compiled in this manner can only run as
Pepper plugins in Chrome. To publish and distribute Native Client modules as
part of a web application, you must eventually use a toolchain in the Native
Client SDK to create ``.nexe`` files.

.. Note::
  :class: note

  * Documentation on how to compile and run modules as Pepper plugins will be
    published soon.
  * In the future, additional tools will be available to compile Native Client
    modules written in other programming languages, such as C#. But this
    document covers only compiling C and C++ code, using the modified GNU
    toolchains provided in the SDK.

Tools
=====

The Native Client toolchains contain modified versions of the tools in the
standard GNU toolchain, including the gcc compilers (currently version 4.4) and
the linkers and other tools from binutils (currently version 2.20).

In the toolchain for the ARM target architecture, each tool's name is preceded
by the prefix "arm-nacl-".

In the toolchains for the x86 target architecture, there are actually two
versions of each tool—one to build Native Client modules for the x86-32 target
architecture, and one to build modules for the x86-64 target architecture. Each
tool's name is preceded by one of the following prefixes:

i686-nacl-
  prefix for tools used to build 32-bit .nexes

x86_64-nacl-
  prefix for tools used to build 64-bit .nexes

These prefixes conform to gcc naming standards and make it easy to use tools
like autoconf. As an example, you can use ``i686-nacl-gcc`` to compile 32-bit
.nexes, and ``x86_64-nacl-gcc`` to compile 64-bit .nexes. Note that you can
typically override a tool's default target architecture with command line
flags, e.g., you can specify ``x86_64-nacl-gcc -m32`` to compile a 32-bit
.nexe.

The SDK toolchains include the following tools:

* <prefix>addr2line
* <prefix>ar
* <prefix>as
* <prefix>c++
* <prefix>c++filt
* <prefix>cpp
* <prefix>g++
* <prefix>gcc
* <prefix>gcc-4.4.3
* <prefix>gccbug
* <prefix>gcov
* <prefix>gprof
* <prefix>ld
* <prefix>nm
* <prefix>objcopy
* <prefix>objdump
* <prefix>ranlib
* <prefix>readelf
* <prefix>size
* <prefix>strings
* <prefix>strip

Compiling
=========

To create a .nexe file, use a compiler in one of the Native Client toolchains.

For example, assuming you're developing on a Windows machine, targeting the x86
architecture, and using the newlib library, you can compile a 32-bit .nexe for
the hello_world example with the following command::

  <NACL_SDK_ROOT>/toolchain/win_x86_newlib/bin/i686-nacl-gcc hello_world.c ^
    -o hello_world_x86_32.nexe -m32 -g -O0 -lppapi

(The carat ``^`` allows the command to span multiple lines on Windows; to do the
same on Mac and Linux use a back slash instead. Or you can simply type the
command and all its arguments on one line. ``<NACL_SDK_ROOT>`` represents the
path to the top-level directory of the bundle you are using, e.g.,
``<location-where-you-installed-the-SDK>/pepper_28``.)

To compile a 64-bit .nexe, you can run the same command but use -m64 instead of
-m32. Alternatively, you could also use the version of the compiler that
targets the x86-64 architecture, i.e., ``x86_64-nacl-gcc``.

You should name executable modules with a ``.nexe`` filename extension,
regardless of what platform you're using.

Compile flags for different development scenarios
=================================================

To optimize the performance of your .nexe module, you must use the correct set
of flags when you compile with nacl-gcc. If you're used to working with an IDE
rather than with a command-line compiler like gcc, you may not be familiar with
which flags you need to specify. The table below summarizes which flags to
specify based on different development scenarios.

===================== =================================================================
Development scenarios Flags for nacl-gcc
===================== =================================================================
debugging             -g -O0
profile               [-g] -O{2|3} -msse -mfpmath=sse -ffast-math -fomit-frame-pointer
deploy                -s -O{2|3} -msse -mfpmath=sse -ffast-math -fomit-frame-pointer
===================== =================================================================

A few of these flags are described below:

-g
  Produce debugging information.

-On
  Optimize the executable for both performance and code size. A higher n
  increases the level of optimization. Use -O0 when debugging, and -O2 or -O3
  for profiling and deployment.

  The main difference between -O2 and -O3 is whether the compiler performs
  optimizations that involve a space-speed tradeoff. It could be the case that
  these optimizations are not desirable due to .nexe download size; you should
  make your own performance measurements to determine which level of
  optimization is right for you. When looking at code size, note that what you
  generally care about is not the size of the .nexe produced by nacl-gcc, but
  the size of the compressed .nexe that you upload to the Chrome Web Store.
  Optimizations that increase the size of a .nexe may not increase the size of
  the compressed .nexe that much.

  For additional information about optimizations, see the `gcc optimization
  options <http://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html>`_. Note
  that the -Os option (optimize for size) is not currently supported.

-s
  Strip the .nexe, i.e., remove all symbol table and relocation information
  from the executable.

  As an alternative to using the -s option, you can store a copy of the
  non-stripped .nexe somewhere so that you can extract symbol information from
  it if necessary, and use the nacl-strip tool in the SDK to strip symbol
  information from the .nexe that you deploy.

.. Note::
  :class: note

  To see how the examples in the SDK are built, run make in any of the example
  subdirectories (e.g., examples/hello_world). The make tool displays the full
  command lines it runs for each step of the build process (compiling, linking,
  and generating Native Client manifest files).

For additional information about compiler options, see `gcc command options
<http://gcc.gnu.org/onlinedocs/gcc/Invoking-GCC.html>`_.

Using make
==========

This document doesn't cover how to use ``make``, but if you want to use
``make`` to build your Native Client module, you can base your Makefile on the
ones in the SDK examples.

The Makefiles for the SDK examples build most of the examples in multiple
configurations (using different C libraries, targeting different architectures,
and using different levels of optimization). With a few exceptions (tumbler,
debugging, and dlopen), running ``make`` in each example's directory does the
following:

* creates a subdirectory called ``newlib``;

  * builds .nexes for the x86-32, x86-64, and ARM architectures using the
    newlib library;
  * generates a Native Client manifest (.nmf) file for the newlib version of
    the example;

* creates a subdirectory called ``glibc``;

  * builds .nexes for the x86-32 and x86-64 architectures using the glibc
    library;
  * generates a Native Client manifest (.nmf) file for the glibc version of the
    example;

* creates a subdirectory called ``windows``, ``linux``, or ``mac`` (depending
  on your development machine);

  * builds a Pepper plugin (.dll for Windows, .so for Linux/Mac) using the
    hosted toolchain on your development machine;
  * generates a Native Client manifest (.nmf) file for the glibc version of the
    example;

* creates a subdirectory called ``pnacl``;

  * builds a .pexe (architecture-independent Native Client executable) using
    the newlib library; and
  * generates a Native Client manifest (.nmf) file for the pnacl version of the
    example;

.. Note::
  :class: note

  * The examples are also built using different optimization levels, and the
    executable and manifest files are actually located in subdirectories called
    "Debug" and "Release".
  * The glibc library is not yet available for the ARM and PNaCl toolchains.
  * Chrome does not yet directly support .pexe files, but the PNaCl toolchain
    contains a tool to translate .pexes into .nexes.

  Your Makefile can be simpler since you will not likely want to build so many
  different configurations of your module. The example Makefiles define
  numerous variables near the top (e.g., ``GLIBC_CCFLAGS``) that make it easy
  to customize the commands that are executed for your project and the options
  for each command.

  In addition to building .nexe files, the example Makefiles also generate
  Native Client manifest (.nmf) files, which your application points to from
  the ``src`` attribute of an ``<embed>`` tag in its HTML file. For information
  about Native Client manifest files, see the :ref:`Technical Overview
  <application_files>`. The SDK includes a script called ``create_nmf.py`` (in
  the ``tools/`` directory) that you can use to generate .nmf files. Run
  "``python create_nmf.py --help``" to see the script's command-line options,
  and look at the Makefiles in the SDK examples to see how to use the script to
  generate a manifest file for modules compiled with either toolchain.

  For details on how to use make, see the `GNU 'make' Manual
  <http://www.gnu.org/software/make/manual/make.html>`_.

Libraries and header files provided with the SDK
================================================

The Native Client SDK includes modified versions of standard toolchain-support
libraries, such as iberty, nosys, pthread, and valgrind, plus the relevant
header files.

The libraries are located in the following directories:

* x86 toolchains: toolchain/<platform>_x86_<library>/x86_64-nacl/lib32 and
  /lib64 (for the 32-bit and 64-bit target architectures, respectively)
* ARM toolchain: toolchain/<platform>_arm_<library>/arm-nacl/lib

For example, on Windows, the libraries for the x86-64 architecture in the
newlib toolchain are in toolchain/win_x86_newlib/x86_64-nacl/lib64.

The standard gcc libraries are also available, in
toolchain/<platform>_<architecture>_<library>/lib.

The header files are in:

* x86 toolchains: toolchain/<platform>_x86_<library>/x86_64-nacl/include
* ARM toolchain: toolchain/<platform>_arm_<library>/arm-nacl/include

The toolchains intentionally leave out some standard libraries and header
files; in particular, for sandboxing reasons, the SDK doesn't support some
POSIX-specified items. For example, ``open(2)`` isn't included, and
``close(2)`` doesn't precisely match the POSIX version.

Many other libraries have been ported for use with Native Client; for more
information, see the `naclports <http://code.google.com/p/naclports/>`_
project. If you port an open-source library for your own use, we recommend
adding it to naclports.

Here are descriptions of some of the Native Client-specific libraries provided
in the SDK:

libppapi.a
  Implements the Pepper (PPAPI) C interface (needed for all applications that
  use Pepper).

libppapi_cpp.a
  Implements the Pepper (PPAPI) C++ interface.

libpthread.a
  Implements the Native Client pthread interface.

libsrpc.a
  Implements the Native Client RPC layer, and is used to implement the Pepper C
  layer.

libimc.a
  Implements the intermodule communications layer (IMC), which is used to
  implement SRPC, the Native Client RPC library.

libgio.a
  Used to implement Native Client logging and some other features in
  nacl_platform.

libplatform.a
  Provides some platform abstractions, and is used to implement some other
  Native Client libraries.

The top-level /lib directory contains two additional Native Client libraries of
interest:

libnacl_mounts.a
  Provides a virtual file system that a module can "mount" in a given directory
  tree. Once a module has mounted a file system, it can use standard C library
  file operations: fopen, fread, fwrite, fseek, and fclose. For a list of the
  types of file systems that can be mounted, see
  include/nacl_mounts/nacl_mounts.h. For an example of how to use nacl_mounts,
  see examples/hello_nacl_mounts.

libppapi_main.a
  Provides a familiar C programming environment by letting a module have a
  simple entry point called ppapi_main(), which is similar to the standard C
  main() function, complete with argc and argv[] parameters. This library also
  lets modules use standard C functions such as printf(), fopen(), and
  fwrite(). For details see include/ppapi_main/ppapi_main.h. For an example of
  how to use ppapi_main, see examples/hello_world_stdio.

.. Note::
  :class: note

  * Since the Native Client toolchains use their own library and header search
    paths, the tools won't find third-party libraries you use in your
    non-Native-Client development. If you want to use a specific third-party
    library for Native Client development, look for it in `naclports
    <http://code.google.com/p/naclports/>`_, or port the library yourself.
  * The order in which you list libraries in your build commands is important,
    since the linker searches and processes libraries in the order in which they
    are specified. See the \*_LDFLAGS variables in the Makefiles of the SDK
    examples for the order in which specific libraries should be listed.

Troubleshooting
===============

Some common problems, and how to fix them:

"Undefined reference" error
  An "undefined reference" error may indicate incorrect link order and/or
  missing libraries. For example, if you leave out -lppapi when compiling the
  hello_world example, you'll see a series of undefined reference errors.

  One common type of "undefined reference" error is with respect to certain
  system calls, e.g., "undefined reference to 'mkdir'". For security reasons,
  Native Client does not support a number of system calls. Depending on how
  your code uses such system calls, you have a few options:

  #. Link with the -lnosys flag to provide empty/always-fail versions of
     unsupported system calls. This will at least get you past the link stage.
  #. Find and remove use of the unsupported system calls.
  #. Create your own implementation of the unsupported system calls to do
     something useful for your application.

  If your code uses mkdir or other file system calls, you might find nacl-mounts
  useful. Nacl-mounts essentially does option (3) for you: It lets your code
  use POSIX-like file system calls, and implements the calls using various
  technologies (e.g., App Engine or an in-memory filesystem).

Can't find libraries containing necessary symbols
  Here is one way to find the appropriate library for a given symbol::

    nm -o toolchain/<platform>_x86_<library>/x86_64-nacl/lib64/*.a | grep <MySymbolName>
