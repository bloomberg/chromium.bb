#include <linux/autoconf.h>

#ifndef CONFIG_SMP
#define CONFIG_SMP 0
#endif

#ifndef CONFIG_MODVERSIONS
#define CONFIG_MODVERSIONS 0
#endif

SMP = CONFIG_SMP
MODVERSIONS = CONFIG_MODVERSIONS
