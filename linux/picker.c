#include <linux/autoconf.h>
#include <linux/version.h>

#ifndef CONFIG_SMP
#define CONFIG_SMP 0
#endif

#ifndef CONFIG_MODVERSIONS
#define CONFIG_MODVERSIONS 0
#endif

SMP = CONFIG_SMP
MODVERSIONS = CONFIG_MODVERSIONS
RELEASE = UTS_RELEASE
