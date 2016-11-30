# Building Chromium on Mandriva

Generally speaking, follow the [Linux Build Instructions](linux_build_instructions.md).
However, do the following instead to install the build dependencies:

## Install the build dependencies:

    urpmi lib64fontconfig-devel lib64alsa2-devel lib64dbus-1-devel \
    lib64GConf2-devel lib64freetype6-devel lib64atk1.0-devel lib64gtk+2.0_0-devel \
    lib64pango1.0-devel lib64cairo-devel lib64nss-devel lib64nspr-devel g++ python \
    perl bison flex subversion gperf

*   msttcorefonts are not available, you will need to build your own (see
instructions, not hard to do, see
[mandriva_msttcorefonts.md](mandriva_msttcorefonts.md)) or use drakfont to
import the fonts from a windows installation
*  These packages are for 64 bit, to download the 32 bit packages,
substitute lib64 with lib
*  Some of these packages might not be explicitly necessary as they come as
   dependencies, there is no harm in including them however.
