#include <linux/config.h>
#include <linux/version.h>

#ifndef CONFIG_SMP
#define CONFIG_SMP 0
#endif

#ifndef CONFIG_MODVERSIONS
#define CONFIG_MODVERSIONS 0
#endif

#ifndef CONFIG_AGP_MODULE
#define CONFIG_AGP_MODULE 0
#endif

#ifndef CONFIG_AGP
#define CONFIG_AGP 0
#endif

#ifndef CONFIG_FB_SIS
#define CONFIG_FB_SIS 0
#endif

SMP = CONFIG_SMP
MODVERSIONS = CONFIG_MODVERSIONS
AGP = CONFIG_AGP
AGP_MODULE = CONFIG_AGP_MODULE
RELEASE = UTS_RELEASE
SIS = CONFIG_FB_SIS
