.. _sdk-examples-2:

Running the SDK Examples
========================

Every Native Client SDK bundle comes with a folder of example applications.
Each example demonstrates one or two key Native Client programming concepts.
After you've :doc:`downloaded the SDK <download>`, follow the instructions
on this page to build and run the examples.

Configure the Google Chrome Browser
-----------------------------------

#. Your version of Chrome must be equal to or greater than the version of
   your SDK bundle. For example, if you're developing with the ``pepper_31``
   bundle, you must use Google Chrome version 31 or greater. To find out what
   version of Chrome you're using, type ``about:chrome`` or ``about:version``
   in the Chrome address bar.

#. For Portable Native Client, no extra Chrome flags are needed as of
   Chrome version 31.

   For other Native Client applications, or to **debug** Portable Native
   Client applications by translating the **pexe** to a **nexe** ahead of
   time, enable the Native Client flag. Native Client is enabled by default
   only for applications distributed through the Chrome Web Store. To run
   Native Client applications that are not distributed through the Chrome
   Web Store, like the SDK examples, you must specifically enable the Native
   Client flag in Chrome:

   * Type ``about:flags`` in the Chrome address bar and scroll down to
     "Native Client".
   * If the link below "Native Client" says "Disable", then Native Client is
     already enabled and you don't need to do anything else.
   * If the link below "Native Client" says "Enable", click the "Enable"
     link, scroll down to the bottom of the page, and click the "Relaunch
     Now" button. All browser windows will restart when you relaunch Chrome.

#. Disable the Chrome cache. Chrome caches resources aggressively; when you
   are building a Native Client application you should disable the cache to
   make sure that Chrome loads the latest version:

   * Open Chrome's developer tools by clicking the menu icon |menu-icon| and
     choosing Tools > Developer tools.
   * Click the gear icon |gear-icon| in the bottom right corner of the
     Chrome window.
   * Under the "General" settings, check the box next to "Disable cache".

Build the SDK examples
----------------------

Starting with the ``pepper_24`` bundle, the Makefile scripts for the SDK
examples build multiple versions of the examples using all three SDK
toolchains (newlib, glibc, and PNaCl) and in both release and debug
configurations.  (Note that some examples build only with the particular
toolchains).

To build all the examples, go to the examples directory in a specific SDK
bundle and run ``make``::

  $ cd pepper_31/examples
  $ make
  make -C api  all
  make[1]: Entering directory `pepper_31/examples/api'
  make -C audio  all
  make[2]: Entering directory `pepper_31/examples/api/audio'
    CXX  newlib/Debug/audio_x86_32.o
    LINK newlib/Debug/audio_x86_32.nexe
    CXX  newlib/Debug/audio_x86_64.o
    LINK newlib/Debug/audio_x86_64.nexe
    CXX  newlib/Debug/audio_arm.o
    LINK newlib/Debug/audio_arm.nexe
    CREATE_NMF newlib/Debug/audio.nmf
  make[2]: Leaving directory `pepper_31/examples/api/audio'
  make -C url_loader  all
  make[2]: Entering directory `pepper_31/examples/api/url_loader'
    CXX  newlib/Debug/url_loader_x86_32.o
  ...

Calling ``make`` from inside a particular example's directory will build only
that example::

  $ cd pepper_31/examples/api/core
  $ make
    CXX  newlib/Debug/core_x86_32.o
    LINK newlib/Debug/core_x86_32.nexe
    CXX  newlib/Debug/core_x86_64.o
    LINK newlib/Debug/core_x86_64.nexe
    CXX  newlib/Debug/core_arm.o
    LINK newlib/Debug/core_arm.nexe
    CREATE_NMF newlib/Debug/core.nmf

You can call ``make`` with the ``TOOLCHAIN`` and ``CONFIG`` parameters to
override the defaults::

  $ make TOOLCHAIN=pnacl CONFIG=Release
    CXX  pnacl/Release/core_pnacl.o
    LINK pnacl/Release/core.bc
    FINALIZE pnacl/Release/core.pexe
    CREATE_NMF pnacl/Release/core.nmf


You can also set ``TOOLCHAIN`` to "all" to build one or more examples with
all available toolchains::

  $ make TOOLCHAIN=all
  make TOOLCHAIN=newlib
  make[1]: Entering directory `pepper_31/examples/api/core'
    CXX  newlib/Debug/core_x86_32.o
    LINK newlib/Debug/core_x86_32.nexe
    CXX  newlib/Debug/core_x86_64.o
    LINK newlib/Debug/core_x86_64.nexe
    CXX  newlib/Debug/core_arm.o
    LINK newlib/Debug/core_arm.nexe
    CREATE_NMF newlib/Debug/core.nmf
  make[1]: Leaving directory `pepper_31/examples/api/core'
  make TOOLCHAIN=glibc
  make[1]: Entering directory `pepper_31/examples/api/core'
    CXX  glibc/Debug/core_x86_32.o
    LINK glibc/Debug/core_x86_32.nexe
    CXX  glibc/Debug/core_x86_64.o
    LINK glibc/Debug/core_x86_64.nexe
    CREATE_NMF glibc/Debug/core.nmf
  make[1]: Leaving directory `pepper_31/examples/api/core'
  make TOOLCHAIN=pnacl
  make[1]: Entering directory `pepper_31/examples/api/core'
    CXX  pnacl/Debug/core.o
    LINK pnacl/Debug/core_unstripped.bc
    FINALIZE pnacl/Debug/core_unstripped.pexe
    CREATE_NMF pnacl/Debug/core.nmf
  make[1]: Leaving directory `pepper_31/examples/api/core'
  make TOOLCHAIN=linux
  make[1]: Entering directory `pepper_31/examples/api/core'
    CXX  linux/Debug/core.o
    LINK linux/Debug/core.so
  make[1]: Leaving directory `pepper_31/examples/api/core'


After running ``make``, each example directory will contain one or more of
the following subdirectories:

* a ``newlib`` directory with subdirectories ``Debug`` and ``Release``;
* a ``glibc`` directory with subdirectories ``Debug`` and ``Release``;
* a ``pnacl`` directory with subdirectories ``Debug`` and ``Release``;

For the newlib and glibc toolchains the Debug and Release subdirectories
contain .nexe files for all target architectures. For the PNaCl toolchain
they contain a single .pexe file. PNaCl debug also produces pre-translated
.nexe files, for ease of debugging. All Debug and Release directories contain
a manifest (.nmf) file that references the associated .nexe or .pexe files.
For information about Native Client manifest files, see the :doc:`Technical
Overview <../overview>`.

For details on how to use ``make``, see the `GNU 'make' Manual
<http://www.gnu.org/software/make/manual/make.html>`_. For details on how to
use the SDK toolchain itself, see :doc:`Building Native Client Modules
<../devguide/devcycle/building>`.

.. _running_the_sdk_examples:

Run the SDK examples
--------------------

To run the SDK examples, you can use the ``make run`` command::

  $ cd pepper_31/examples/api/core
  $ make run

This will launch a local HTTP server which will serve the data for the
example. It then launches Chrome with the address of this server, usually
``http://localhost:5103``. After you close Chrome, the local HTTP server is
automatically shutdown.

This command will try to find an executable named ``google-chrome`` in your
``PATH`` environment variable. If it can't, you'll get an error message like
this::

  pepper_31/tools/common.mk:415: No valid Chrome found at CHROME_PATH=
  pepper_31/tools/common.mk:415: *** Set CHROME_PATH via an environment variable, or command-line..  Stop.

Set the CHROME_PATH environment variable to the location of your Chrome
executable.

* On Windows:

  The default install location of Chrome is
  ``C:\Program Files (x86)\Google\Chrome\Application\chrome.exe`` for Chrome
  stable and
  ``C:\Users\<username>\AppData\Local\Google\Chrome SxS\Application\chrome.exe``
  for Chrome Canary; try looking in those directories first::

    > set CHROME_PATH=<Path to chrome.exe>

* On Linux::

    $ export CHROME_PATH=<Path to google-chrome>

* On Mac:

  The default install location of Chrome is
  ``/Applications/Google Chrome.app/Contents/MacOS/Google Chrome`` for
  Chrome Stable and
  ``Applications/Google Chrome Canary.app/Contents/MacOS/Google Chrome Canary``
  for Chrome Canary. Note that you have to reference the executable inside the
  application bundle, not the top-level ``.app`` directory::

    $ export CHROME_PATH=<Path to Google Chrome>

You can run via a different toolchain or configuration by using the
``TOOLCHAIN`` and ``CONFIG`` parameters to make::

  $ make run TOOLCHAIN=pnacl CONFIG=Debug

.. _run_sdk_examples_as_packaged:

Run the SDK examples as packaged apps
-------------------------------------

Each example can also be launched as a packaged app. For more information about
using Native Client for packaged apps, see :ref:`Packaged application
<distributing_packaged>`.  For general information about packaged apps, see the
`Chrome apps documentation </apps/about_apps>`_.

Some Pepper features, such as TCP/UDP socket access, are only allowed in
packaged apps. The examples that use these features must be run as packaged
apps, by using the ``make run_package`` command::

  $ make run_package

You can use ``TOOLCHAIN`` and ``CONFIG`` parameters as above to run with a
different toolchain or configuration.


.. _debugging_the_sdk_examples:

Debugging the SDK examples
--------------------------

The NaCl SDK uses `GDB <https://www.gnu.org/software/gdb/>`_ to debug Native
Client code. The SDK includes a prebuilt version of GDB that is compatible with
NaCl code. To use it, run the ``make debug`` command from an example directory::

  $ make debug

This will launch Chrome with the ``--enable-nacl-debug`` flag set. This flag
will cause Chrome to pause when a NaCl module is first loaded, waiting for a
connection from gdb. The ``make debug`` command also simultaneously launches
GDB and loads the symbols for that NEXE. To connect GDB to Chrome, in the GDB
console, type::

  (gdb) target remote :4014

This tells GDB to connect to a TCP port on ``localhost:4014``--the port that
Chrome is listening on. GDB will respond::

  Remote debugging using :4014
  0x000000000fa00080 in ?? ()

At this point, you can use the standard GDB commands to debug your NaCl module.
The most common commands you will use to debug are ``continue``, ``step``,
``next``, ``break`` and ``backtrace``. See :doc:`Debugging
<../devguide/devcycle/debugging>` for more information about debugging a Native Client
application.


.. |menu-icon| image:: /images/menu-icon.png
.. |gear-icon| image:: /images/gear-icon.png
