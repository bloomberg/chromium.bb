.. _sdk-index:

#################
Native Client SDK
#################

To build Native Client modules, you must download and install the Native
Client Software Development Kit (SDK).

The SDK includes the following:

support for multiple Pepper versions
  The SDK contains **bundles** that let you compile Native Client modules
  using different versions of the
  `Pepper API </overview.html#link_pepper>`_
  (e.g., Pepper 30 or Pepper Canary). Review the
  :doc:`Release Notes <release-notes>` for a description of the new features
  included in each Pepper version to help you decide which bundle to
  use to develop your application. In general, Native Client modules
  compiled using a particular Pepper version will work in
  corresponding versions of Chrome and higher. For example, a module
  compiled using the Pepper 30 platform will work in Chrome 30 and
  higher.

update utility
  The ``naclsdk`` utility (``naclsdk.bat`` on Windows) lets you download new
  bundles that are available, as well as new versions of existing bundles.

toolchains
  Each platform includes three toolchains---one for Portable Native Client
  (PNaCl) applications, one for compiling architecture-specific Native
  Client applications with newlib, and a third native NaCl toolchain for
  compiling with glibc. Newlib and glibc are two different implementations
  of the C standard library. All toolchains contain Native Client-compatible
  versions of standard compilers, linkers, and other tools.
  See :doc:`this document </nacl-and-pnacl>` to help you choose the
  right toolchain.

examples
  Each example in the SDK includes C or C++ source files and header files
  illustrating how to use NaCl and Pepper, along with a Makefile to build
  the example using each of the toolchains.

tools
  The SDK includes a number of additional tools, which you can use for
  tasks such as validating Native Client modules and running modules
  from the command line.

To download and install the SDK, follow the instructions on the
:ref:`Download <download>` page.

