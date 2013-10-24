######################################
Dynamic Linking and Loading with glibc
######################################

.. contents::
  :local:
  :backlinks: none
  :depth: 2

Introduction
============

.. Note::
  :class: caution

  Portable Native Client currently only supports static linking, and the
  only C library available for it is newlib. This page is only valid for
  Native Client, though PNaCl will eventually support some form of
  dynamic linking.

This document describes how to create and deploy dynamically linked and loaded
applications with the glibc library in the Native Client SDK. Before reading
this document, we recommend reading :doc:`Building Native Client Modules
<building>`

.. _c_libraries:

C standard libraries: glibc and newlib
--------------------------------------

The Native Client SDK comes with two C standard libraries --- glibc and
newlib.  These libraries are described in the table below.

+--------+----------+-------------+--------------------------------------------+
| Library| Linking  | License     | Description                                |
+========+==========+=============+============================================+
| glibc  | dynamic  | GNU Lesser  | glibc is the GNU implementation of the     |
|        | or static| General     | POSIX_ standard runtime library for the C  |
|        |          | Public      | programming language. Designed for         |
|        |          | License     | portability and performance, glibc is one  |
|        |          | (LGPL)      | of the most popular implementations of the |
|        |          |             | C library. It is comprised of a set of     |
|        |          |             | interdependent libraries including libc,   |
|        |          |             | libpthreads, libdl, and others. For        |
|        |          |             | documentation, FAQs, and additional        |
|        |          |             | information about glibc, see GLIBC_        |
+--------+----------+-------------+--------------------------------------------+
| newlib | static   | Berkeley    | newlib is a C library intended for use in  |
|        |          | Software    | embedded systems. Like glibc, newlib is a  |
|        |          | Distribution| conglomeration of several library parts.   |
|        |          | (BSD) type  | It is available for use under BSD-type free|
|        |          | free        | software licenses, which generally makes it|
|        |          | software    | more suitable to link statically in        |
|        |          | licenses    | commercial, closed-source applications. For|
|        |          |             | documentation, FAQs, and additional        |
|        |          |             | information about newlib, see the `newlib  |
|        |          |             | documentation`__.                          |
+--------+----------+-------------+--------------------------------------------+

.. _GLIBC: http://www.gnu.org/software/libc/index.html
.. _POSIX: http://en.wikipedia.org/wiki/POSIX
__ http://sourceware.org/newlib

For proprietary (closed-source) applications, your options are to either
statically link to newlib, or dynamically link to glibc. We recommend
dynamically linking to glibc, for a couple of reasons:

* The glibc library is widely distributed (it's included in Linux
  distributions), and as such it's mature, hardened, and feature-rich. Your
  code is more likely to compile out-of-the-box with glibc.

* Dynamic loading can provide a big performance benefit for your application if
  you can structure the application to defer loading of code that's not needed
  for initial interaction with the user. It takes some work to put such code in
  shared libraries and to load the libraries at runtime, but the payoff is
  usually worth it. In future releases, Chrome may also support caching of
  common dynamically linked libraries such as libc.so between applications.
  This could significantly reduce download size and provide a further potential
  performance benefit (for example, the hello_world example would only require
  downloading a .nexe file that's on the order of 30KB, rather than a .nexe
  file and several libraries, which are on the order of 1.5MB).

Native Client support for dynamic linking and loading is based on glibc. Thus,
**if your Native Client application must dynamically link and load code (e.g.,
due to licensing considerations), we recommend that you use the glibc
library.**

.. Note::
  :class: note

  **Notes:**

  * **None of the above constitutes legal advice, or a description of the legal
    obligations you need to fulfill in order to be compliant with the LGPL or
    newlib licenses. The above description is only a technical explanation of
    the differences between newlib and glibc, and the choice you must make
    between the two libraries.**

  * Static linking with glibc is rarely used. Use this feature with caution.

  * The standard C++ runtime in Native Client is provided by libstdc++; this
    library is independent from and layered on top of glibc. Because of
    licensing restrictions, libstdc++ must be statically linked for commercial
    uses, even if the rest of an application is dynamically linked.

SDK toolchains
--------------

The Native Client SDK contains multiple toolchains, which are differentiated by
:ref:`target architecture <target_architectures>` and C library:

=================== ========= ===============================
Target architecture C library Toolchain directory
=================== ========= ===============================
x86                 newlib    toolchain/<platform>_x86_newlib
x86                 glibc     toolchain/<platform>_x86_glibc
ARM                 newlib    toolchain/<platform>_arm_newlib
PNaCl               newlib    toolchain/<platform>_pnacl
=================== ========= ===============================

In the directories listed above, <platform> is the platform of your development
machine (i.e., win, mac, or linux). For example, in the Windows SDK, the x86
toolchain that uses glibc is in ``toolchain/win_x86_glibc``.

.. Note::
  :class: note

  **Note:** The ARM and PNaCl toolchains are currently restricted to newlib.

To use the glibc library and dynamic linking in your application, you **must**
use a glibc toolchain. (Currently the only glibc toolchain is
``<platform>_x86_glibc``.) Note that you must build all code in your application
with one toolchain. Code from multiple toolchains cannot be mixed.

Specifying and delivering shared libraries
------------------------------------------

One significant difference between newlib and glibc applications is that glibc
applications must explicitly list and deploy the shared libraries that they
use.

In a desktop environment, when the user launches a dynamically linked
application, the operating system's program loader determines the set of
libraries the application requires by reading explicit inter-module
dependencies from executable file headers, and loads the required libraries
into the address space of the application process. Typically the required
libraries will have been installed on the system as a part of the application's
installation process. Often the desktop application developer doesn't know or
think about the libraries that are required by an application, as those details
are taken care of by the user's operating system.

In the Native Client sandbox, dynamic linking can't rely in the same way on the
operating system or the local file system. Instead, the application developer
must identify the set of libraries that are required by an application, list
those libraries in a Native Client :ref:`manifest file <manifest_file>`, and
deploy the libraries along with the application. Instructions for how to build
a dynamically linked Native Client application, generate a Native Client
manifest (.nmf) file, and deploy an application are provided below.

Building a dynamically linked application
=========================================

A dynamically linked application typically includes one Native Client module
and one or more shared libraries. (How to allocate code between Native Client
modules and shared libraries is a question of application design that is beyond
the scope of this document.) Each Native Client module and shared library must
be compiled for at least the x86 32-bit and 64-bit architectures.

The dlopen example in the SDK
-----------------------------

The Native Client SDK includes an example that demonstrates how to build a
shared library, and how to use the ``dlopen()`` interface to load that library
at runtime (after the application is already running). Many applications load
and link shared libraries at launch rather than at runtime, and hence do not
use the ``dlopen()`` interface. The SDK example is nevertheless instructive, as
it demonstrates how to build Native Client modules (.nexe files) and shared
libraries (.so files) with the x86 glibc toolchain, and how to generate a
Native Client manifest file for glibc applications.

The SDK example, located in the directory examples/dlopen, includes two C++
files:

eightball.cc
  This file implements the function ``Magic8Ball()``, which is used to provide
  whimsical answers to user questions. The file is compiled into a shared
  library, ``libeightball.so``.

dlopen.cc
  This file implements the Native Client module, which loads
  ``libeightball.so``, receives messages from JavaScript (sent in response to
  user input), calls ``Magic8Ball()`` to generate answers, and sends messages
  back to JavaScript with the generated answers. The file is compiled into a
  .nexe file.

.. TODO(sbc): also mention reverse.{cc,h} files

Run ``make`` in the dlopen directory to see the commands the Makefile executes
to build x86 32-bit and 64-bit .nexe and .so files, and to generate a .nmf
file. These commands are described below.

.. Note::
  :class: note

  **Note:** The Makefiles for most of the examples in the SDK build the
  examples using multiple toolchains (x86 newlib, x86 glibc, ARM, and PNaCl).
  With a few exceptions (listed in the :ref:`Release Notes
  <sdk-release-notes>`), running "make" in each example's directory builds
  multiple versions of the example using the SDK toolchains. The dlopen example
  is one of those exceptions – it is only built with the x86 glibc toolchain,
  as that is currently the only toolchain that supports glibc and thus dynamic
  linking and loading. Take a look at the example Makefiles and the generated
  .nmf files for details on how to build dynamically linked applications.


Building a Native Client module (.nexe file)
--------------------------------------------

.. TODO(sbc): there is a lot of redundant detail here.  Also the Makefile
   structure has changed significantly.

The Makefile in the dlopen example builds ``dlopen.cc`` into a .nexe file using
the two commands shown below. (For simplicity, the full path to the
compiler/linker is not shown; the tool is located in the bin directory in the
x86 glibc toolchain, e.g. toolchain/win_x86_glibc/bin.)

To compile dlopen.cc into dlopen_x86_32.o::

  i686-nacl-g++ -o dlopen_x86_32.o -c dlopen.cc -m32 -g -O0 -pthread -std=gnu++98 -Wno-long-long -Wall

To link dlopen_x86_32.o into dlopen_x86_32.nexe::

  i686-nacl-g++ -o dlopen_x86_32.nexe dlopen_x86_32.o -m32 -g -ldl -lppapi_cpp -lppapi

A few of the flags in these commands are described below:

``-o`` *file*
  put the output in *file*

``-c``
  compile the source file, but do not link it

``-m32``
  produce 32-bit code (i.e., code for the x86-32 target architecture)

``-g``
  produce debugging information

``-O0``
  use a base optimization level that minimizes compile time

``-pthread``
  support multithreading with the pthread library

``-W`` *warning*
  request or supress the specified warning

``-l`` *library*
  use the specified *library* when linking (per C library naming conventions,
  the linker uses the file lib*library*.so, or if that file is not available,
  lib*library*.a; e.g., -ldl corresponds to libdl.so or libdl.a)

Many of these flags are optional; you need not use all of them to compile and
link your application. For example, you only need to use -ldl if your
application uses the dlopen() interface to open a library at runtime. The
toolchains in the Native Client SDK are based on the gcc compiler; see `gcc
command options <http://gcc.gnu.org/onlinedocs/gcc/Invoking-GCC.html>`_ for a
full description of the gcc flags. For flags that are recommended with Native
Client, see :ref:`compile flags for different development scenarios
<compile_flags>`.

Note that you can combine the compile and link steps to build a .nexe file
using one command. Simply run i686-nacl-g++ once and use the appropriate
combination of flags (omit the -c flag and include the -l flag with the
required libraries)::

  i686-nacl-g++ -o dlopen_x86_32.nexe dlopen.cc ^ -m32 -g -O0 -pthread -std=gnu++98 -Wno-long-long -Wall  -ldl -lppapi_cpp -lppapi

(The carat ``^`` allows the command to span multiple lines on Windows; to do
the same on Mac and Linux use a backslash instead. Or you can simply type the
command and all its arguments on one line.)

The commands above build a 32-bit .nexe. To build a 64-bit .nexe, run the same
commands but with the **-m64** flag instead of -m32, and of course specify
different output file names. Check the Makefile in the dlopen example to see
the set of commands that is used to generate 32-bit and 64-bit .nexes.

Building a shared library (.so file)
------------------------------------

The Makefile in the dlopen example builds eightball.cc into a .so file using
the two commands shown below.

To compile eightball.cc into eightball_x86_32.o::

  i686-nacl-g++ -o eightball_x86_32.o -c eightball.cc -m32 -g -O0 -pthread -std=gnu++98 -Wno-long-long -Wall -fPIC

To link eightball_x86_32.o into eightball_x86_32.so::

  i686-nacl-g++ -o libeightball.so eightball_x86_32.o -m32 -g -ldl -lppapi_cpp -lppapi -shared

A couple of the important flags in these commands are described below:

``-fPIC``
  generate position-independent code (PIC) suitable for use in a shared library
  (this flag is required for all x86 64-bit modules and for 32-bit shared
  libraries)
``-shared``
  produce a shared object that can be linked with other objects to form an
  executable (this flag is required for .so files)
  As when building a .nexe, you can combine compiling and linking into one step
  by running i686-nacl-g++ once with the appropriate combination of flags.

As with .nexes, you need to generate both 32-bit and 64-bit versions of a
shared object -- see the dlopen example for an illustration. In the dlopen
example, the shared objects are put into the subdirectories ``lib32`` and
``lib64``.  These directories are used to collect all the shared libraries
needed by the application, as discussed below.

.. _dynamic_loading_manifest:

Generating a Native Client manifest file for a dynamically linked application
=============================================================================

The Native Client manifest file must specify the full list of executable files
needed by an application, including the recursive closure of shared library
dependencies. Take a look at the manifest file in the dlopen example to see how
a glibc-style manifest file is structured. (Run make in the dlopen directory to
generate the manifest file if you haven't done so already.) Here is an excerpt
from ``dlopen.nmf``::

  {
    "files": {
      "libeightball.so": {
        "x86-64": {
          "url": "lib64/libeightball.so"
        },
        "x86-32": {
          "url": "lib32/libeightball.so"
        }
      },
      "libstdc++.so.6": {
        "x86-64": {
          "url": "lib64/libstdc++.so.6"
        },
        "x86-32": {
          "url": "lib32/libstdc++.so.6"
        }
      },
      "libppapi_cpp.so": {
        "x86-64": {
          "url": "lib64/libppapi_cpp.so"
        },
        "x86-32": {
          "url": "lib32/libppapi_cpp.so"
        }
      },
  ... etc.

In most cases, you can use the ``create_nmf.py`` script in the SDK to generate
a manifest file for your application. The script is located in the tools
directory (e.g., pepper_28/tools).

.. TODO(sbc): running create_nmf.py is much simpler now.

The Makefile in the dlopen example generates the manifest file ``dlopen.nmf``
by running the following command::

  python <NACL_SDK_ROOT>/tools/create_nmf.py ^
    -D <NACL_SDK_ROOT>/toolchain/win_x86_glibc/x86_64-nacl/bin/objdump ^
    -o dlopen.nmf ^
    -s . ^
    dlopen_x86_32.nexe dlopen_x86_64.nexe lib32/libeightball.so lib64/libeightball.so ^
    -L <NACL_SDK_ROOT>/toolchain/win_x86_glibc/x86_64-nacl/lib32 ^
    -L <NACL_SDK_ROOT>/toolchain/win_x86_glibc/x86_64-nacl/lib64

(The carat ``^`` allows the command to span multiple lines on Windows; to do the
same on Mac and Linux use a backslash instead, or you can simply type the
command and all its arguments on one line. *<NACL_SDK_ROOT>* represents the path
to the top-level directory of the bundle you are using, e.g.,
*<location-where-you-installed-the-SDK>*/pepper_28.)


Run python ``create_nmf.py --help`` to see a description of the command-line
flags. A few of the important flags are described below.

.. TODO(sbc): remove -D option which is now deprecated.

``-D`` *tool*
  use *tool* to read information about a file and determine shared library
  dependencies (the tool must be a version of the `objdump
  <http://en.wikipedia.org/wiki/Objdump>`_ utility)

``-s`` *directory*
  use *directory* to stage libraries (libraries are added to ``lib32`` and
  ``lib64`` subfolders)

``-L`` *directory*
  add *directory* to the library search path

.. Note::
  :class: note

  **Caution:** The ``create_nmf.py`` script only recognizes explicit shared
  library dependencies (for example, dependencies specified with the -l flag
  for the compiler/linker). The manifest file generated by create_nmf.py will
  be incorrect in the following situations:

  * You run ``create_nmf.py`` without listing as arguments all the libraries
    that your application opens with ``dlopen()``.

  * After you run ``create_nmf.py``, you subsequently add a library dependency
    that is not mentioned in the manifest file.

  * After you run ``create_nmf.py``, you subsequently change the directory
    structure on your server or in your Chrome Web Store manifest file, such
    that the needed libraries are no longer in the location specified in the
    .nmf file

  To handle the above situations correctly, you must re-run ``create_nmf.py``
  (for example, if you added a new library dependency in your application or
  changed the application directory structure), and make sure to list all the
  libraries that your application opens at runtime with ``dlopen()`` (e.g.,
  libeighball.so in the dlopen example).

.. TODO(sbc): We probably don't want/need this next section in the docs at all.

As an alternative to using ``create_nmf.py``, you can also chase down the full
list of shared library dependencies manually and add those to your .nmf file.
To do so, start by running the Native Client version of the objdump utility on
your .nexe file, as shown below. (The objdump utility is located in the same
directory as the glibc toolchain, e.g., toolchain/win_x86_glibc/bin.)

::

  i686-nacl-objdump -p dlopen_x86_32.nexe

A .nexe file contains compiled machine code, as well as headers that describe
the contents of the file and information about how to use the file. The objdump
utility lets you examine the file's headers, including the "Dynamic Section,"
which specifies shared library dependencies, as in this example output from the
dlopen example::

  Dynamic Section:
    NEEDED               libdl.so.32d9fc17
    NEEDED               libppapi_cpp.so
    NEEDED               libpthread.so.32d9fc17
    NEEDED               libstdc++.so.6
    NEEDED               libm.so.32d9fc17
    NEEDED               libgcc_s.so.1
    NEEDED               libc.so.32d9fc17
    INIT                 0x01000140
    FINI                 0x01002560
    HASH                 0x110025fc
    ...


All the files that are identified as NEEDED in the "Dynamic Section" portion of
the objdump output are files that you need to list in your Native Client
manifest file and distribute with your application. (The numbers listed at the
end of the file names are version numbers, and you must list and distribute
those exact versions.) Once you've identified the shared libraries that are
needed by your .nexe file, you must repeat the process recursively: Run objdump
on each of the NEEDED files, and add the newly-identified NEEDED files to your
manifest file and to your distribution directories. To get the full list of
libraries for an application, repeat the process until you've identified the
recursive closure of dependencies.

Deploying a dynamically linked application
==========================================

As described above, an application's manifest file must explicitly list all the
executable code modules that the application requires, including modules from
the application itself (.nexe and .so files), modules from the Native Client
SDK (e.g., libppapi_cpp.so), and perhaps also modules from `naclports
<http://code.google.com/p/naclports>`_ or from :doc:`middleware systems
<../../community/middleware>` that the application uses. You must provide all of
those modules as part of the application deployment process.

As explained in :doc:`Distributing Your Application
<../distributing>`, there are two basic ways to deploy an application:

* **hosted application:** all modules are hosted together on a web server of
  your choice

* **packaged application:** all modules are packaged into one file, hosted in
  the Chrome Web Store, and downloaded to the user's machine

You must deploy all the modules listed in your application's manifest file for
either the hosted application or the packaged application case. For hosted
applications, you must upload the modules to your web server. For packaged
applications, you must include the modules in the application's Chrome Web
Store .crx file. Modules should use URLs/names that are consistent with those
in the Native Client manifest file, and be named relative to the location of
the manifest file. Remember that some of the libraries named in the manifest
file may be located in directories you specified with the -L option to
``create_nmf.py``. You are free to rename/rearrange files and directories
referenced by the Native Client manifest file, so long as the modules are
available in the locations indicated by the manifest file. If you move or
rename modules, it may be easier to re-run create_nmf.py to generate a new
manifest file rather than edit the original manifest file. For hosted
applications, you can check for name mismatches during testing by watching the
request log of the web server hosting your test deployment.

Opening a shared library at runtime
===================================

Native Client supports a version of the POSIX standard ``dlopen()`` interface
for opening libraries explicitly, after an application is already running.
Calling ``dlopen()`` may cause a library download to occur, and automatically
loads all libraries that are required by the named library.

.. Note::
  :class: note

  **Caution:** Since ``dlopen()`` can potentially block, you must initially
  call ``dlopen()`` off your application's main thread. Initial calls to
  ``dlopen()`` from the main thread will always fail in the current
  implementation of Native Client.

The best practice for opening libraries with ``dlopen()`` is to use a worker
thread to pre-load libraries asynchronously during initialization of your
application, so that the libraries are available when they're needed. You can
call ``dlopen()`` a second time when you need to use a library -- per the
specification, subsequent calls to ``dlopen()`` return a handle to the
previously loaded library.  Note that you should only call dlclose() to close a
library when you no longer need the library; otherwise, subsequent calls to
``dlopen()`` could cause the library to be fetched again.

The dlopen example in the SDK demonstrates how to open a shared library,
magiceightball.so, at runtime. To reiterate, the example includes two C++
files:

.. TODO(sbc): mention the third .cc file which is now part of this example.

* eightball.cc: this is the shared library that implements the function
  ``Magic8Ball()`` (this file is compiled into libeightball.so)
* dlopen.cc: this is the Native Client module that loads ``libeightball.so``
  and calls ``Magic8Ball()`` to generate answers (this file is compiled into
  dlopen_x86_{32,64}.nexe)

When the Native Client module starts, it kicks off a worker thread that calls
``dlopen()`` to load magiceightball.so. When the download of
``libeightball.so`` completes, the worker thread schedules a callback function
on the main thread.  The callback function calls ``dlopen()`` for
``magiceightball.so`` a second time; this second call obtains a proper handle
to the library. Once the module has a handle to the library, it grabs the entry
point in libeightball.so for the ``Magic8Ball()`` function. When a user types
in a query and clicks the 'ASK!' button, the module calls ``Magic8Ball()`` to
generate an answer, and returns the result to the user.

The sequence of calls in the dlopen module is illustrated by the pseudo-code in
the table below:

+------------------------------------------------+------------------------------------------+
| Worker Thread                                  | Main Thread                              |
+================================================+==========================================+
| ::                                             | ::                                       |
|                                                |                                          |
|   pthread_create(.., LoadLibrariesOnWorker, ..)|   -                                      |
|   -                                            |   LoadLibrariesOnWorker()                |
|   -                                            |     LoadLibrary()                        |
|   -                                            |       dlopen("libeightball.so",...)      |
|   -                                            |       CallOnMainThread(.., LoadDone, ..) |
|   LoadDone()                                   |   -                                      |
|     UseLibrary()                               |   -                                      |
|       dlopen("libeightball.so", ...)           |   -                                      |
|       offset = dlsym(..., "Magic8Ball")        |   -                                      |
|   HandleMessage()                              |   -                                      |
|     _eightball = (TYPE_eightball) offset;      |   -                                      |
|     PostMessage()                              |   -                                      |
+------------------------------------------------+------------------------------------------+

Troubleshooting
===============

If your .nexe isn't loading, the best place to look for information that can
help you troubleshoot the problem is stdout and nacllog. See the Debugging page
for instructions about how to access those streams.

Here are a few common error messages and explanations of what they mean:

**/main.nexe: error while loading shared libraries: /main.nexe: failed to allocate code and data space for executable**
  The .nexe may not have been compiled correctly (e.g., the .nexe may be
  statically linked). Try cleaning and recompiling with the glibc toolchain.

**/main.nexe: error while loading shared libraries: libpthread.so.xxxx: cannot open shared object file: Permission denied**
  (xxxx is a version number, for example, 5055067a.) This error can result from
  having the wrong path in the .nmf file. Double-check that the path in the
  .nmf file is correct.

**/main.nexe: error while loading shared libraries: /main.nexe: cannot open shared object file: No such file or directory**
  If there are no obvious problems with your main.nexe entry in the .nmf file,
  check where main.nexe is being requested from. Use Chrome's Developer Tools:
  Click the menu icon |menu-icon|, select Tools > Developer Tools, click the
  Network tab, and look at the path in the Name column.

**NaCl module load failed: ELF executable text/rodata segment has wrong starting address**
  This error happens when using a newlib-style .nmf file instead of a
  glibc-style .nmf file. Make sure you build your application with the glic
  toolchain, and use the create_nmf.py script to generate your .nmf file.

**NativeClient: NaCl module load failed: Nexe crashed during startup**
  This error message indicates that a module crashed while being loaded. You
  can determine which module crashed by looking at the Network tab in Chrome's
  Developer Tools (see above). The module that crashed will be the last one
  that was loaded.

**/lib/main.nexe: error while loading shared libraries: /lib/main.nexe: only ET_DYN and ET_EXEC can be loaded**
  This error message indicates that there is an error with the .so files listed
  in the .nmf file -- either the files are the wrong type or kind, or an
  expected library is missing.

**undefined reference to 'dlopen' collect2: ld returned 1 exit status**
  This is a linker ordering problem that usually results from improper ordering
  of command line flags when linking. Reconfigure your command line string to
  list libraries after the -o flag.

.. |menu-icon| image:: /images/menu-icon.png
