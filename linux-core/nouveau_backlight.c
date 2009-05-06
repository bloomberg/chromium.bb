/*
 * Copyright (C) 2009 Red Hat <mjg@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * Authors:
 *  Matthew Garrett <mjg@redhat.com>
 *
 *  PowerPC specific stuff:
 *  Danny Tholen <moondrake@gmail.com>
 *  (mainly based on info from nvidiafb)
 *
 * Register locations derived from NVClock by Roderick Colenbrander
 */

#include <linux/backlight.h>

#include "drmP.h"
#include "nouveau_drv.h"
#include "nouveau_drm.h"
#include "nouveau_reg.h"
#ifdef CONFIG_PMAC_BACKLIGHT
#include <asm/backlight.h>
#endif

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23))

#ifdef CONFIG_PMAC_BACKLIGHT
static int ppc_get_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int raw_val = (NV_READ(NV_PBUS_DEBUG_DUALHEAD_CTL) & 0x7fff0000) >> 16;
	int min_brightness = bd->props.max_brightness / 3;
	int intensity = raw_val - min_brightness;

	return (intensity < 0 ? 0 : intensity);
}

static int ppc_set_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int val = bd->props.brightness;
	int min_brightness = bd->props.max_brightness / 3;
	int dh_ctl = NV_READ(NV_PBUS_DEBUG_DUALHEAD_CTL) & 0x0000ffff;
	int gpio_ext = NV_READ(NV_PCRTC_GPIO_EXT) & ~0x3;

	if (val > 0) {
		dh_ctl |= (1 << 31); /* backlight bit on */
		dh_ctl |= (val + min_brightness) << 16;
		gpio_ext |= 0x1;
	}

	NV_WRITE(NV_PBUS_DEBUG_DUALHEAD_CTL, dh_ctl);
	NV_WRITE(NV_PCRTC_GPIO_EXT, gpio_ext);

	return 0;
}

static struct backlight_ops ppc_bl_ops = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
	.options = BL_CORE_SUSPENDRESUME,
#endif
	.get_brightness = ppc_get_intensity,
	.update_status = ppc_set_intensity,
};
#endif /* CONFIG_PMAC_BACKLIGHT */

static int nv40_get_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int val = (NV_READ(NV40_PMC_BACKLIGHT) & NV40_PMC_BACKLIGHT_MASK) >> 16;

	return val;
}

static int nv40_set_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int val = bd->props.brightness;
	int reg = NV_READ(NV40_PMC_BACKLIGHT);

	NV_WRITE(NV40_PMC_BACKLIGHT,
		 (val << 16) | (reg & ~NV40_PMC_BACKLIGHT_MASK));

	return 0;
}

static struct backlight_ops nv40_bl_ops = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
	.options = BL_CORE_SUSPENDRESUME,
#endif
	.get_brightness = nv40_get_intensity,
	.update_status = nv40_set_intensity,
};

static int nv50_get_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	return NV_READ(NV50_PDISPLAY_BACKLIGHT);
}

static int nv50_set_intensity(struct backlight_device *bd)
{
	struct drm_device *dev = bl_get_data(bd);
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int val = bd->props.brightness;

	NV_WRITE(NV50_PDISPLAY_BACKLIGHT, val | NV50_PDISPLAY_BACKLIGHT_ENABLE);

	return 0;
}

static struct backlight_ops nv50_bl_ops = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
	.options = BL_CORE_SUSPENDRESUME,
#endif
	.get_brightness = nv50_get_intensity,
	.update_status = nv50_set_intensity,
};

#ifdef CONFIG_PMAC_BACKLIGHT
static int nouveau_ppc_backlight_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct backlight_device *bd;
	int max_brightness;

	if (!machine_is(powermac) ||
	    !pmac_has_backlight_type("mnca"))
		return -ENODEV;

	/* "nv_bl0", some old powerpc userland tools rely on this */
	bd = backlight_device_register("nv_bl0", &dev->pdev->dev, dev,
				       &ppc_bl_ops);
	if (IS_ERR(bd))
		return PTR_ERR(bd);

	dev_priv->backlight = bd;

	/* the least significant 15 bits of NV_PBUS_DEBUG_DUALHEAD_CTL
	 * set the maximum backlight level, OF sets it to 0x535 on my
	 * powerbook. however, posting the card sets this to 0x7FFF.
	 * Below 25% the blacklight turns off.
	 *
	 * NOTE: pbbuttonsd limits brightness values < 255, and thus
	 * will not work well with this.
	 */

	/* FIXME: do we want to fix this reg to 0x535 for consistency? */
	max_brightness = NV_READ(NV_PBUS_DEBUG_DUALHEAD_CTL) & 0x0000ffff;
	/* lose (bottom) 25% of range */
	bd->props.max_brightness = (max_brightness * 3) / 4;
	bd->props.brightness = ppc_get_intensity(bd);
	backlight_update_status(bd);

	return 0;
}
#endif

static int nouveau_nv40_backlight_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct backlight_device *bd;
	
	if (!(NV_READ(NV40_PMC_BACKLIGHT) & NV40_PMC_BACKLIGHT_MASK))
		return 0;

	bd = backlight_device_register("nv_backlight", &dev->pdev->dev, dev,
				       &nv40_bl_ops);
	if (IS_ERR(bd))
		return PTR_ERR(bd);

	dev_priv->backlight = bd;
	bd->props.max_brightness = 31;
	bd->props.brightness = nv40_get_intensity(bd);
	backlight_update_status(bd);

	return 0;
}

static int nouveau_nv50_backlight_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct backlight_device *bd;

	if (!NV_READ(NV50_PDISPLAY_BACKLIGHT))
		return 0;

	bd = backlight_device_register("nv_backlight", &dev->pdev->dev, dev,
				       &nv50_bl_ops);
	if (IS_ERR(bd))
		return PTR_ERR(bd);

	dev_priv->backlight = bd;
	bd->props.max_brightness = 1025;
	bd->props.brightness = nv50_get_intensity(bd);
	backlight_update_status(bd);
	return 0;
}

int nouveau_backlight_init(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	switch (dev_priv->card_type) {
#ifdef CONFIG_PMAC_BACKLIGHT
	case NV_17:
	case NV_30:
		return nouveau_ppc_backlight_init(dev);
#endif
	case NV_40:
	case NV_44:
		return nouveau_nv40_backlight_init(dev);
		break;
	case NV_50:
		return nouveau_nv50_backlight_init(dev);
		break;
	}
	return 0;
}

void nouveau_backlight_exit(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	if (dev_priv->backlight)
		backlight_device_unregister(dev_priv->backlight);
}

#else /* CONFIG_BACKLIGHT_CLASS_DEVICE not set */
int nouveau_backlight_init(struct drm_device *dev)
{
	(void)dev;
	return 0;
}

void nouveau_backlight_exit(struct drm_device *dev)
{
	(void)dev;
}
#endif /* CONFIG_BACKLIGHT_CLASS_DEVICE not set */
