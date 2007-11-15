#include "drmP.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)

#include "i915_drm.h"
#include "i915_drv.h"

#define PCI_DEVICE_ID_INTEL_82946GZ_HB      0x2970
#define PCI_DEVICE_ID_INTEL_82965G_1_HB     0x2980
#define PCI_DEVICE_ID_INTEL_82965Q_HB       0x2990
#define PCI_DEVICE_ID_INTEL_82965G_HB       0x29A0
#define PCI_DEVICE_ID_INTEL_82965GM_HB      0x2A00
#define PCI_DEVICE_ID_INTEL_82965GME_HB     0x2A10
#define PCI_DEVICE_ID_INTEL_82945GME_HB     0x27AC
#define PCI_DEVICE_ID_INTEL_G33_HB          0x29C0
#define PCI_DEVICE_ID_INTEL_Q35_HB          0x29B0
#define PCI_DEVICE_ID_INTEL_Q33_HB          0x29D0

#define I915_IFPADDR    0x60
#define I965_IFPADDR    0x70

static struct _i9xx_private_compat {
	void __iomem *flush_page;
	struct resource ifp_resource;
} i9xx_private;

static struct _i8xx_private_compat {
	void *flush_page;
	struct page *page;
} i8xx_private;

static void
intel_compat_align_resource(void *data, struct resource *res,
                        resource_size_t size, resource_size_t align)
{
	return;
}


static int intel_alloc_chipset_flush_resource(struct pci_dev *pdev)
{
	int ret;
	ret = pci_bus_alloc_resource(pdev->bus, &i9xx_private.ifp_resource, PAGE_SIZE,
				     PAGE_SIZE, PCIBIOS_MIN_MEM, 0,
				     intel_compat_align_resource, pdev);
	if (ret != 0)
		return ret;

	return 0;
}

static void intel_i915_setup_chipset_flush(struct pci_dev *pdev)
{
	int ret;
	u32 temp;

	pci_read_config_dword(pdev, I915_IFPADDR, &temp);
	if (!(temp & 0x1)) {
		intel_alloc_chipset_flush_resource(pdev);

		pci_write_config_dword(pdev, I915_IFPADDR, (i9xx_private.ifp_resource.start & 0xffffffff) | 0x1);
	} else {
		temp &= ~1;

		i9xx_private.ifp_resource.start = temp;
		i9xx_private.ifp_resource.end = temp + PAGE_SIZE;
		ret = request_resource(&iomem_resource, &i9xx_private.ifp_resource);
		if (ret) {
			i9xx_private.ifp_resource.start = 0;
			printk("Failed inserting resource into tree\n");
		}
	}
}

static void intel_i965_g33_setup_chipset_flush(struct pci_dev *pdev)
{
	u32 temp_hi, temp_lo;
	int ret;

	pci_read_config_dword(pdev, I965_IFPADDR + 4, &temp_hi);
	pci_read_config_dword(pdev, I965_IFPADDR, &temp_lo);

	if (!(temp_lo & 0x1)) {

		intel_alloc_chipset_flush_resource(pdev);

		pci_write_config_dword(pdev, I965_IFPADDR + 4, (i9xx_private.ifp_resource.start >> 32));
		pci_write_config_dword(pdev, I965_IFPADDR, (i9xx_private.ifp_resource.start & 0xffffffff) | 0x1);
	} else {
		u64 l64;

		temp_lo &= ~0x1;
		l64 = ((u64)temp_hi << 32) | temp_lo;

		i9xx_private.ifp_resource.start = l64;
		i9xx_private.ifp_resource.end = l64 + PAGE_SIZE;
		ret = request_resource(&iomem_resource, &i9xx_private.ifp_resource);
		if (!ret) {
			i9xx_private.ifp_resource.start = 0;
			printk("Failed inserting resource into tree\n");
		}
	}
}

static void intel_i8xx_fini_flush(struct drm_device *dev)
{
	kunmap(i8xx_private.page);
	i8xx_private.flush_page = NULL;
	unmap_page_from_agp(i8xx_private.page);
	flush_agp_mappings();

	__free_page(i8xx_private.page);
}

static void intel_i8xx_setup_flush(struct drm_device *dev)
{

	i8xx_private.page = alloc_page(GFP_KERNEL | __GFP_ZERO | GFP_DMA32);
	if (!i8xx_private.page) {
		return;
	}

	/* make page uncached */
	map_page_into_agp(i8xx_private.page);
	flush_agp_mappings();

	i8xx_private.flush_page = kmap(i8xx_private.page);
	if (!i8xx_private.flush_page)
		intel_i8xx_fini_flush(dev);

	DRM_ERROR("i8xx got flush page %p %p\n", i8xx_private.page, i8xx_private.flush_page);
}


static void intel_i8xx_flush_page(struct drm_device *dev)
{
	unsigned int *pg = i8xx_private.flush_page;
	int i;

	/* HAI NUT CAN I HAZ HAMMER?? */
	for (i = 0; i < 256; i++)
		*(pg + i) = i;
	
	DRM_MEMORYBARRIER();
}

static void intel_i9xx_setup_flush(struct drm_device *dev)
{
	struct pci_dev *agp_dev = dev->agp->agp_info.device;

	i9xx_private.ifp_resource.name = "GMCH IFPBAR";
	i9xx_private.ifp_resource.flags = IORESOURCE_MEM;

	/* Setup chipset flush for 915 */
	if (IS_I965G(dev) || IS_G33(dev)) {
		intel_i965_g33_setup_chipset_flush(agp_dev);
	} else {
		intel_i915_setup_chipset_flush(agp_dev);
	}

	if (i9xx_private.ifp_resource.start) {
		i9xx_private.flush_page = ioremap_nocache(i9xx_private.ifp_resource.start, PAGE_SIZE);
		if (!i9xx_private.flush_page)
			printk("unable to ioremap flush  page - no chipset flushing");
	}
}

static void intel_i9xx_fini_flush(struct drm_device *dev)
{
	iounmap(i9xx_private.flush_page);
	release_resource(&i9xx_private.ifp_resource);
}

static void intel_i9xx_flush_page(struct drm_device *dev)
{
	if (i9xx_private.flush_page)
		writel(1, i9xx_private.flush_page);
}

void intel_init_chipset_flush_compat(struct drm_device *dev)
{
	/* not flush on i8xx */
	if (IS_I9XX(dev))	
		intel_i9xx_setup_flush(dev);
	else
		intel_i8xx_setup_flush(dev);
	
}

void intel_fini_chipset_flush_compat(struct drm_device *dev)
{
	/* not flush on i8xx */
	if (IS_I9XX(dev))
		intel_i9xx_fini_flush(dev);
	else
		intel_i8xx_fini_flush(dev);
}

void drm_agp_chipset_flush(struct drm_device *dev)
{
	if (IS_I9XX(dev))
		intel_i9xx_flush_page(dev);
	else
		intel_i8xx_flush_page(dev);
}
#endif
