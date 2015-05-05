/*
 * Copyright 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
*/

/**
 * \file amdgpu.h
 *
 * Declare public libdrm_amdgpu API
 *
 * This file define API exposed by libdrm_amdgpu library.
 * User wanted to use libdrm_amdgpu functionality must include
 * this file.
 *
 */
#ifndef _AMDGPU_H_
#define _AMDGPU_H_

#include <stdint.h>
#include <stdbool.h>

struct drm_amdgpu_info_hw_ip;

/*--------------------------------------------------------------------------*/
/* --------------------------- Defines ------------------------------------ */
/*--------------------------------------------------------------------------*/

/**
 * Define max. number of Command Buffers (IB) which could be sent to the single
 * hardware IP to accommodate CE/DE requirements
 *
 * \sa amdgpu_cs_ib_info
*/
#define AMDGPU_CS_MAX_IBS_PER_SUBMIT		4

/**
 *
 */
#define AMDGPU_TIMEOUT_INFINITE			0xffffffffffffffffull

/**
 * The special flag for GFX submission to identify that this is CE IB
 * \sa amdgpu_cs_ib_info
*/
#define AMDGPU_CS_GFX_IB_CE			0x1

/**
 * The special flag to mark that this IB will re-used
 * by client and should not be automatically return back
 * to free pool by libdrm_amdgpu when submission is completed.
 *
 * \sa amdgpu_cs_ib_info
*/
#define AMDGPU_CS_REUSE_IB			0x2

/**
 * The special resource flag for IB submission.
 * When VRAM is full, some resources may be moved to GTT to make place
 * for other resources which want to be in VRAM. This flag affects the order
 * in which resources are moved back to VRAM until there is no space there.
 * The resources with the highest priority will be moved first.
 * The value can be between 0 and 15, inclusive.
 */
#define AMDGPU_IB_RESOURCE_PRIORITY(x)		((x) & 0xf)


/*--------------------------------------------------------------------------*/
/* ----------------------------- Enums ------------------------------------ */
/*--------------------------------------------------------------------------*/

/**
 * Enum describing possible handle types
 *
 * \sa amdgpu_bo_import, amdgpu_bo_export
 *
*/
enum amdgpu_bo_handle_type {
	/** GEM flink name (needs DRM authentication, used by DRI2) */
	amdgpu_bo_handle_type_gem_flink_name = 0,

	/** KMS handle which is used by all driver ioctls */
	amdgpu_bo_handle_type_kms = 1,

	/** DMA-buf fd handle */
	amdgpu_bo_handle_type_dma_buf_fd = 2
};

/**
 * Enum describing possible context reset states
 *
 * \sa amdgpu_cs_query_reset_state()
 *
*/
enum amdgpu_cs_ctx_reset_state {
	/** No reset was detected */
	amdgpu_cs_reset_no_error = 0,

	/** Reset/TDR was detected and context caused */
	amdgpu_cs_reset_guilty   = 1,

	/** Reset/TDR was detected caused by other context */
	amdgpu_cs_reset_innocent = 2,

	/** Reset TDR was detected by cause of it unknown */
	amdgpu_cs_reset_unknown  = 3
};

/**
 * For performance reasons and to simplify logic libdrm_amdgpu will handle
 * IBs only some pre-defined sizes.
 *
 * \sa amdgpu_cs_alloc_ib()
 */
enum amdgpu_cs_ib_size {
	amdgpu_cs_ib_size_4K	= 1,
	amdgpu_cs_ib_size_16K	= 2,
	amdgpu_cs_ib_size_32K	= 3,
	amdgpu_cs_ib_size_64K	= 4,
	amdgpu_cs_ib_size_128K	= 5
};

/** The number of different IB sizes */
#define AMDGPU_CS_IB_SIZE_NUM 6


/*--------------------------------------------------------------------------*/
/* -------------------------- Datatypes ----------------------------------- */
/*--------------------------------------------------------------------------*/

/**
 * Define opaque pointer to context associated with fd.
 * This context will be returned as the result of
 * "initialize" function and should be pass as the first
 * parameter to any API call
 */
typedef struct amdgpu_device *amdgpu_device_handle;

/**
 * Define GPU Context type as pointer to opaque structure
 * Example of GPU Context is the "rendering" context associated
 * with OpenGL context (glCreateContext)
 */
typedef struct amdgpu_context *amdgpu_context_handle;

/**
 * Define handle for amdgpu resources: buffer, GDS, etc.
 */
typedef struct amdgpu_bo *amdgpu_bo_handle;

/**
 * Define handle for list of BOs
 */
typedef struct amdgpu_bo_list *amdgpu_bo_list_handle;

/**
 * Define handle to be used when dealing with command
 * buffers (a.k.a. ibs)
 *
 */
typedef struct amdgpu_ib *amdgpu_ib_handle;


/*--------------------------------------------------------------------------*/
/* -------------------------- Structures ---------------------------------- */
/*--------------------------------------------------------------------------*/

/**
 * Structure describing memory allocation request
 *
 * \sa amdgpu_bo_alloc()
 *
*/
struct amdgpu_bo_alloc_request {
	/** Allocation request. It must be aligned correctly. */
	uint64_t alloc_size;

	/**
	 * It may be required to have some specific alignment requirements
	 * for physical back-up storage (e.g. for displayable surface).
	 * If 0 there is no special alignment requirement
	 */
	uint64_t phys_alignment;

	/**
	 * UMD should specify where to allocate memory and how it
	 * will be accessed by the CPU.
	 */
	uint32_t preferred_heap;

	/** Additional flags passed on allocation */
	uint64_t flags;
};

/**
 * Structure describing memory allocation request
 *
 * \sa amdgpu_bo_alloc()
*/
struct amdgpu_bo_alloc_result {
	/** Assigned virtual MC Base Address */
	uint64_t virtual_mc_base_address;

	/** Handle of allocated memory to be used by the given process only. */
	amdgpu_bo_handle buf_handle;
};

/**
 * Special UMD specific information associated with buffer.
 *
 * It may be need to pass some buffer charactersitic as part
 * of buffer sharing. Such information are defined UMD and
 * opaque for libdrm_amdgpu as well for kernel driver.
 *
 * \sa amdgpu_bo_set_metadata(), amdgpu_bo_query_info,
 *     amdgpu_bo_import(), amdgpu_bo_export
 *
*/
struct amdgpu_bo_metadata {
	/** Special flag associated with surface */
	uint64_t flags;

	/**
	 * ASIC-specific tiling information (also used by DCE).
	 * The encoding is defined by the AMDGPU_TILING_* definitions.
	 */
	uint64_t tiling_info;

	/** Size of metadata associated with the buffer, in bytes. */
	uint32_t size_metadata;

	/** UMD specific metadata. Opaque for kernel */
	uint32_t umd_metadata[64];
};

/**
 * Structure describing allocated buffer. Client may need
 * to query such information as part of 'sharing' buffers mechanism
 *
 * \sa amdgpu_bo_set_metadata(), amdgpu_bo_query_info(),
 *     amdgpu_bo_import(), amdgpu_bo_export()
*/
struct amdgpu_bo_info {
	/** Allocated memory size */
	uint64_t alloc_size;

	/**
	 * It may be required to have some specific alignment requirements
	 * for physical back-up storage.
	 */
	uint64_t phys_alignment;

	/**
	 * Assigned virtual MC Base Address.
	 * \note  This information will be returned only if this buffer was
	 * allocated in the same process otherwise 0 will be returned.
	*/
	uint64_t virtual_mc_base_address;

	/** Heap where to allocate memory. */
	uint32_t preferred_heap;

	/** Additional allocation flags. */
	uint64_t alloc_flags;

	/** Metadata associated with buffer if any. */
	struct amdgpu_bo_metadata metadata;
};

/**
 * Structure with information about "imported" buffer
 *
 * \sa amdgpu_bo_import()
 *
 */
struct amdgpu_bo_import_result {
	/** Handle of memory/buffer to use */
	amdgpu_bo_handle  buf_handle;

	 /** Buffer size */
	uint64_t alloc_size;

	 /** Assigned virtual MC Base Address */
	uint64_t virtual_mc_base_address;
};


/**
 *
 * Structure to describe GDS partitioning information.
 * \note OA and GWS resources are asscoiated with GDS partition
 *
 * \sa amdgpu_gpu_resource_query_gds_info
 *
*/
struct amdgpu_gds_resource_info {
	uint32_t   gds_gfx_partition_size;
	uint32_t   compute_partition_size;
	uint32_t   gds_total_size;
	uint32_t   gws_per_gfx_partition;
	uint32_t   gws_per_compute_partition;
	uint32_t   oa_per_gfx_partition;
	uint32_t   oa_per_compute_partition;
};



/**
 *  Structure describing result of request to allocate GDS
 *
 *  \sa amdgpu_gpu_resource_gds_alloc
 *
*/
struct amdgpu_gds_alloc_info {
	/** Handle assigned to gds allocation */
	amdgpu_bo_handle resource_handle;

	/** How much was really allocated */
	uint32_t   gds_memory_size;

	/** Number of GWS resources allocated */
	uint32_t   gws;

	/** Number of OA resources allocated */
	uint32_t   oa;
};

/**
 * Structure to described allocated command buffer (a.k.a. IB)
 *
 * \sa amdgpu_cs_alloc_ib()
 *
*/
struct amdgpu_cs_ib_alloc_result {
	/** IB allocation handle */
	amdgpu_ib_handle handle;

	/** Assigned GPU VM MC Address of command buffer */
	uint64_t	mc_address;

	/** Address to be used for CPU access */
	void		*cpu;
};

/**
 * Structure describing IB
 *
 * \sa amdgpu_cs_request, amdgpu_cs_submit()
 *
*/
struct amdgpu_cs_ib_info {
	/** Special flags */
	uint64_t      flags;

	/** Handle of command buffer */
	amdgpu_ib_handle ib_handle;

	/**
	 * Size of Command Buffer to be submitted.
	 *   - The size is in units of dwords (4 bytes).
	 *   - Must be less or equal to the size of allocated IB
	 *   - Could be 0
	 */
	uint32_t       size;
};

/**
 * Structure describing submission request
 *
 * \note We could have several IBs as packet. e.g. CE, CE, DE case for gfx
 *
 * \sa amdgpu_cs_submit()
*/
struct amdgpu_cs_request {
	/** Specify flags with additional information */
	uint64_t	flags;

	/** Specify HW IP block type to which to send the IB. */
	unsigned	ip_type;

	/** IP instance index if there are several IPs of the same type. */
	unsigned	ip_instance;

	/**
	 * Specify ring index of the IP. We could have several rings
	 * in the same IP. E.g. 0 for SDMA0 and 1 for SDMA1.
	 */
	uint32_t           ring;

	/**
	 * List handle with resources used by this request.
	 */
	amdgpu_bo_list_handle resources;

	/** Number of IBs to submit in the field ibs. */
	uint32_t number_of_ibs;

	/**
	 * IBs to submit. Those IBs will be submit together as single entity
	 */
	struct amdgpu_cs_ib_info *ibs;
};

/**
 * Structure describing request to check submission state using fence
 *
 * \sa amdgpu_cs_query_fence_status()
 *
*/
struct amdgpu_cs_query_fence {

	/** In which context IB was sent to execution */
	amdgpu_context_handle  context;

	/** Timeout in nanoseconds. */
	uint64_t  timeout_ns;

	/** To which HW IP type the fence belongs */
	unsigned  ip_type;

	/** IP instance index if there are several IPs of the same type. */
	unsigned ip_instance;

	/** Ring index of the HW IP */
	uint32_t      ring;

	/** Flags */
	uint64_t  flags;

	/** Specify fence for which we need to check
	 * submission status.*/
	uint64_t	fence;
};

/**
 * Structure which provide information about GPU VM MC Address space
 * alignments requirements
 *
 * \sa amdgpu_query_buffer_size_alignment
 */
struct amdgpu_buffer_size_alignments {
	/** Size alignment requirement for allocation in
	 * local memory */
	uint64_t size_local;

	/**
	 * Size alignment requirement for allocation in remote memory
	 */
	uint64_t size_remote;
};


/**
 * Structure which provide information about heap
 *
 * \sa amdgpu_query_heap_info()
 *
 */
struct amdgpu_heap_info {
	/** Theoretical max. available memory in the given heap */
	uint64_t  heap_size;

	/**
	 * Number of bytes allocated in the heap. This includes all processes
	 * and private allocations in the kernel. It changes when new buffers
	 * are allocated, freed, and moved. It cannot be larger than
	 * heap_size.
	 */
	uint64_t  heap_usage;

	/**
	 * Theoretical possible max. size of buffer which
	 * could be allocated in the given heap
	 */
	uint64_t  max_allocation;
};



/**
 * Describe GPU h/w info needed for UMD correct initialization
 *
 * \sa amdgpu_query_gpu_info()
*/
struct amdgpu_gpu_info {
	/** Asic id */
	uint32_t asic_id;
	/**< Chip revision */
	uint32_t chip_rev;
	/** Chip external revision */
	uint32_t chip_external_rev;
	/** Family ID */
	uint32_t family_id;
	/** Special flags */
	uint64_t ids_flags;
	/** max engine clock*/
	uint64_t max_engine_clk;
	/** number of shader engines */
	uint32_t num_shader_engines;
	/** number of shader arrays per engine */
	uint32_t num_shader_arrays_per_engine;
	/**  Number of available good shader pipes */
	uint32_t avail_quad_shader_pipes;
	/**  Max. number of shader pipes.(including good and bad pipes  */
	uint32_t max_quad_shader_pipes;
	/** Number of parameter cache entries per shader quad pipe */
	uint32_t cache_entries_per_quad_pipe;
	/**  Number of available graphics context */
	uint32_t num_hw_gfx_contexts;
	/** Number of render backend pipes */
	uint32_t rb_pipes;
	/**  Enabled render backend pipe mask */
	uint32_t enabled_rb_pipes_mask;
	/** Frequency of GPU Counter */
	uint32_t gpu_counter_freq;
	/** CC_RB_BACKEND_DISABLE.BACKEND_DISABLE per SE */
	uint32_t backend_disable[4];
	/** Value of MC_ARB_RAMCFG register*/
	uint32_t mc_arb_ramcfg;
	/** Value of GB_ADDR_CONFIG */
	uint32_t gb_addr_cfg;
	/** Values of the GB_TILE_MODE0..31 registers */
	uint32_t gb_tile_mode[32];
	/** Values of GB_MACROTILE_MODE0..15 registers */
	uint32_t gb_macro_tile_mode[16];
	/** Value of PA_SC_RASTER_CONFIG register per SE */
	uint32_t pa_sc_raster_cfg[4];
	/** Value of PA_SC_RASTER_CONFIG_1 register per SE */
	uint32_t pa_sc_raster_cfg1[4];
	/* CU info */
	uint32_t cu_active_number;
	uint32_t cu_ao_mask;
	uint32_t cu_bitmap[4][4];
};


/*--------------------------------------------------------------------------*/
/*------------------------- Functions --------------------------------------*/
/*--------------------------------------------------------------------------*/

/*
 * Initialization / Cleanup
 *
*/


/**
 *
 * \param   fd            - \c [in]  File descriptor for AMD GPU device
 *                                   received previously as the result of
 *                                   e.g. drmOpen() call.
 *                                   For legacy fd type, the DRI2/DRI3 authentication
 *                                   should be done before calling this function.
 * \param   major_version - \c [out] Major version of library. It is assumed
 *                                   that adding new functionality will cause
 *                                   increase in major version
 * \param   minor_version - \c [out] Minor version of library
 * \param   device_handle - \c [out] Pointer to opaque context which should
 *                                   be passed as the first parameter on each
 *                                   API call
 *
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 *
 * \sa amdgpu_device_deinitialize()
*/
int amdgpu_device_initialize(int fd,
			     uint32_t *major_version,
			     uint32_t *minor_version,
			     amdgpu_device_handle *device_handle);



/**
 *
 * When access to such library does not needed any more the special
 * function must be call giving opportunity to clean up any
 * resources if needed.
 *
 * \param   device_handle - \c [in]  Context associated with file
 *                                   descriptor for AMD GPU device
 *                                   received previously as the
 *                                   result e.g. of drmOpen() call.
 *
 * \return  0 on success\n
 *         >0 - AMD specific error code\n
 *         <0 - Negative POSIX Error code
 *
 * \sa amdgpu_device_initialize()
 *
*/
int amdgpu_device_deinitialize(amdgpu_device_handle device_handle);


/*
 * Memory Management
 *
*/

/**
 * Allocate memory to be used by UMD for GPU related operations
 *
 * \param   dev		 - \c [in] Device handle.
 *				   See #amdgpu_device_initialize()
 * \param   alloc_buffer - \c [in] Pointer to the structure describing an
 *				   allocation request
 * \param   info	 - \c [out] Pointer to structure which return
 *				    information about allocated memory
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_bo_free()
*/
int amdgpu_bo_alloc(amdgpu_device_handle dev,
		    struct amdgpu_bo_alloc_request *alloc_buffer,
		    struct amdgpu_bo_alloc_result *info);

/**
 * Associate opaque data with buffer to be queried by another UMD
 *
 * \param   dev	       - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   buf_handle - \c [in] Buffer handle
 * \param   info       - \c [in] Metadata to associated with buffer
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
*/
int amdgpu_bo_set_metadata(amdgpu_bo_handle buf_handle,
			   struct amdgpu_bo_metadata *info);

/**
 * Query buffer information including metadata previusly associated with
 * buffer.
 *
 * \param   dev	       - \c [in] Device handle.
 *				 See #amdgpu_device_initialize()
 * \param   buf_handle - \c [in]   Buffer handle
 * \param   info       - \c [out]  Structure describing buffer
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_bo_set_metadata(), amdgpu_bo_alloc()
*/
int amdgpu_bo_query_info(amdgpu_bo_handle buf_handle,
			 struct amdgpu_bo_info *info);

/**
 * Allow others to get access to buffer
 *
 * \param   dev		  - \c [in] Device handle.
 *				    See #amdgpu_device_initialize()
 * \param   buf_handle    - \c [in] Buffer handle
 * \param   type          - \c [in] Type of handle requested
 * \param   shared_handle - \c [out] Special "shared" handle
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_bo_import()
 *
*/
int amdgpu_bo_export(amdgpu_bo_handle buf_handle,
		     enum amdgpu_bo_handle_type type,
		     uint32_t *shared_handle);

/**
 * Request access to "shared" buffer
 *
 * \param   dev		  - \c [in] Device handle.
 *				    See #amdgpu_device_initialize()
 * \param   type	  - \c [in] Type of handle requested
 * \param   shared_handle - \c [in] Shared handle received as result "import"
 *				     operation
 * \param   output        - \c [out] Pointer to structure with information
 *				     about imported buffer
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \note  Buffer must be "imported" only using new "fd" (different from
 *	  one used by "exporter").
 *
 * \sa amdgpu_bo_export()
 *
*/
int amdgpu_bo_import(amdgpu_device_handle dev,
		     enum amdgpu_bo_handle_type type,
		     uint32_t shared_handle,
		     struct amdgpu_bo_import_result *output);

/**
 * Free previosuly allocated memory
 *
 * \param   dev	       - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   buf_handle - \c [in]  Buffer handle to free
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \note In the case of memory shared between different applications all
 *	 resources will be “physically” freed only all such applications
 *	 will be terminated
 * \note If is UMD responsibility to ‘free’ buffer only when there is no
 *	 more GPU access
 *
 * \sa amdgpu_bo_set_metadata(), amdgpu_bo_alloc()
 *
*/
int amdgpu_bo_free(amdgpu_bo_handle buf_handle);

/**
 * Request CPU access to GPU accessable memory
 *
 * \param   buf_handle - \c [in] Buffer handle
 * \param   cpu        - \c [out] CPU address to be used for access
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_bo_cpu_unmap()
 *
*/
int amdgpu_bo_cpu_map(amdgpu_bo_handle buf_handle, void **cpu);

/**
 * Release CPU access to GPU memory
 *
 * \param   buf_handle  - \c [in] Buffer handle
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_bo_cpu_map()
 *
*/
int amdgpu_bo_cpu_unmap(amdgpu_bo_handle buf_handle);


/**
 * Wait until a buffer is not used by the device.
 *
 * \param   dev           - \c [in] Device handle. See #amdgpu_lib_initialize()
 * \param   buf_handle    - \c [in] Buffer handle.
 * \param   timeout_ns    - Timeout in nanoseconds.
 * \param   buffer_busy   - 0 if buffer is idle, all GPU access was completed
 *                            and no GPU access is scheduled.
 *                          1 GPU access is in fly or scheduled
 *
 * \return   0 - on success
 *          <0 - AMD specific error code
 */
int amdgpu_bo_wait_for_idle(amdgpu_bo_handle buf_handle,
			    uint64_t timeout_ns,
			    bool *buffer_busy);

/**
 * Creates a BO list handle for command submission.
 *
 * \param   dev			- \c [in] Device handle.
 *				   See #amdgpu_device_initialize()
 * \param   number_of_resources	- \c [in] Number of BOs in the list
 * \param   resources		- \c [in] List of BO handles
 * \param   resource_prios	- \c [in] Optional priority for each handle
 * \param   result		- \c [out] Created BO list handle
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_bo_list_destroy()
*/
int amdgpu_bo_list_create(amdgpu_device_handle dev,
			  uint32_t number_of_resources,
			  amdgpu_bo_handle *resources,
			  uint8_t *resource_prios,
			  amdgpu_bo_list_handle *result);

/**
 * Destroys a BO list handle.
 *
 * \param   handle	- \c [in] BO list handle.
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_bo_list_create()
*/
int amdgpu_bo_list_destroy(amdgpu_bo_list_handle handle);

/*
 * Special GPU Resources
 *
*/



/**
 * Query information about GDS
 *
 * \param   dev	     - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   gds_info - \c [out] Pointer to structure to get GDS information
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_gpu_resource_query_gds_info(amdgpu_device_handle dev,
					struct amdgpu_gds_resource_info *
								gds_info);


/**
 * Allocate GDS partitions
 *
 * \param   dev	       - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   gds_size   - \c [in] Size of gds allocation. Must be aligned
 *				accordingly.
 * \param   alloc_info - \c [out] Pointer to structure to receive information
 *				  about allocation
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 *
*/
int amdgpu_gpu_resource_gds_alloc(amdgpu_device_handle dev,
				   uint32_t gds_size,
				   struct amdgpu_gds_alloc_info *alloc_info);




/**
 * Release GDS resource. When GDS and associated resources not needed any
 * more UMD should free them
 *
 * \param   dev	   - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   handle - \c [in] Handle assigned to GDS allocation
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_gpu_resource_gds_free(amdgpu_bo_handle handle);



/*
 * GPU Execution context
 *
*/

/**
 * Create GPU execution Context
 *
 * For the purpose of GPU Scheduler and GPU Robustness extensions it is
 * necessary to have information/identify rendering/compute contexts.
 * It also may be needed to associate some specific requirements with such
 * contexts.  Kernel driver will guarantee that submission from the same
 * context will always be executed in order (first come, first serve).
 *
 *
 * \param   dev	    - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   context - \c [out] GPU Context handle
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_cs_ctx_free()
 *
*/
int amdgpu_cs_ctx_create(amdgpu_device_handle dev,
			 amdgpu_context_handle *context);

/**
 *
 * Destroy GPU execution context when not needed any more
 *
 * \param   context - \c [in] GPU Context handle
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_cs_ctx_create()
 *
*/
int amdgpu_cs_ctx_free(amdgpu_context_handle context);

/**
 * Query reset state for the specific GPU Context
 *
 * \param   context - \c [in]  GPU Context handle
 * \param   state   - \c [out] Reset state status
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_cs_ctx_create()
 *
*/
int amdgpu_cs_query_reset_state(amdgpu_context_handle context,
				enum amdgpu_cs_ctx_reset_state *state);


/*
 * Command Buffers Management
 *
*/


/**
 * Allocate memory to be filled with PM4 packets and be served as the first
 * entry point of execution (a.k.a. Indirect Buffer)
 *
 * \param   context - \c [in]  GPU Context which will use IB
 * \param   ib_size - \c [in]  Size of allocation
 * \param   output  - \c [out] Pointer to structure to get information about
 *				   allocated IB
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \sa amdgpu_cs_free_ib()
 *
*/
int amdgpu_cs_alloc_ib(amdgpu_context_handle context,
		       enum amdgpu_cs_ib_size ib_size,
		       struct amdgpu_cs_ib_alloc_result *output);

/**
 * If UMD has allocates IBs which doesn’t need any more than those IBs must
 * be explicitly freed
 *
 * \param   handle  - \c [in] IB handle
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \note Libdrm_amdgpu will guarantee that it will correctly detect when it
 *	is safe to return IB to free pool
 *
 * \sa amdgpu_cs_alloc_ib()
 *
*/
int amdgpu_cs_free_ib(amdgpu_ib_handle handle);

/**
 * Send request to submit command buffers to hardware.
 *
 * Kernel driver could use GPU Scheduler to make decision when physically
 * sent this request to the hardware. Accordingly this request could be put
 * in queue and sent for execution later. The only guarantee is that request
 * from the same GPU context to the same ip:ip_instance:ring will be executed in
 * order.
 *
 *
 * \param   dev		       - \c [in]  Device handle.
 *					  See #amdgpu_device_initialize()
 * \param   context            - \c [in]  GPU Context
 * \param   flags              - \c [in]  Global submission flags
 * \param   ibs_request        - \c [in]  Pointer to submission requests.
 *					  We could submit to the several
 *					  engines/rings simulteniously as
 *					  'atomic' operation
 * \param   number_of_requests - \c [in]  Number of submission requests
 * \param   fences             - \c [out] Pointer to array of data to get
 *					  fences to identify submission
 *					  requests. Timestamps are valid
 *					  in this GPU context and could be used
 *					  to identify/detect completion of
 *					  submission request
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \note It is assumed that by default IB will be returned to free pool
 *	 automatically by libdrm_amdgpu when submission will completed.
 *	 It is possible for UMD to make decision to re-use the same IB in
 *	 this case it should be explicitly freed.\n
 *	 Accordingly, by default, after submission UMD should not touch passed
 *	 IBs. If UMD needs to re-use IB then the special flag AMDGPU_CS_REUSE_IB
 *	 must be passed.
 *
 * \note It is required to pass correct resource list with buffer handles
 *	 which will be accessible by command buffers from submission
 *	 This will allow kernel driver to correctly implement "paging".
 *	 Failure to do so will have unpredictable results.
 *
 * \sa amdgpu_command_buffer_alloc(), amdgpu_command_buffer_free(),
 *     amdgpu_cs_query_fence_status()
 *
*/
int amdgpu_cs_submit(amdgpu_context_handle context,
		     uint64_t flags,
		     struct amdgpu_cs_request *ibs_request,
		     uint32_t number_of_requests,
		     uint64_t *fences);

/**
 *  Query status of Command Buffer Submission
 *
 * \param   dev	    - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   fence   - \c [in] Structure describing fence to query
 * \param   expired - \c [out] If fence expired or not.\n
 *				0  – if fence is not expired\n
 *				!0 - otherwise
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
 * \note If UMD wants only to check operation status and returned immediately
 *	 then timeout value as 0 must be passed. In this case success will be
 *	 returned in the case if submission was completed or timeout error
 *	 code.
 *
 * \sa amdgpu_cs_submit()
*/
int amdgpu_cs_query_fence_status(struct amdgpu_cs_query_fence *fence,
				 uint32_t *expired);


/*
 * Query / Info API
 *
*/


/**
 * Query allocation size alignments
 *
 * UMD should query information about GPU VM MC size alignments requirements
 * to be able correctly choose required allocation size and implement
 * internal optimization if needed.
 *
 * \param   dev  - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   info - \c [out] Pointer to structure to get size alignment
 *			  requirements
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_query_buffer_size_alignment(amdgpu_device_handle dev,
					struct amdgpu_buffer_size_alignments
									*info);



/**
 * Query firmware versions
 *
 * \param   dev	        - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   fw_type     - \c [in] AMDGPU_INFO_FW_*
 * \param   ip_instance - \c [in] Index of the IP block of the same type.
 * \param   index       - \c [in] Index of the engine. (for SDMA and MEC)
 * \param   version     - \c [out] Pointer to to the "version" return value
 * \param   feature     - \c [out] Pointer to to the "feature" return value
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_query_firmware_version(amdgpu_device_handle dev, unsigned fw_type,
				  unsigned ip_instance, unsigned index,
				  uint32_t *version, uint32_t *feature);



/**
 * Query the number of HW IP instances of a certain type.
 *
 * \param   dev      - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   type     - \c [in] Hardware IP block type = AMDGPU_HW_IP_*
 * \param   count    - \c [out] Pointer to structure to get information
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
*/
int amdgpu_query_hw_ip_count(amdgpu_device_handle dev, unsigned type,
			     uint32_t *count);



/**
 * Query engine information
 *
 * This query allows UMD to query information different engines and their
 * capabilities.
 *
 * \param   dev         - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   type        - \c [in] Hardware IP block type = AMDGPU_HW_IP_*
 * \param   ip_instance - \c [in] Index of the IP block of the same type.
 * \param   info        - \c [out] Pointer to structure to get information
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
*/
int amdgpu_query_hw_ip_info(amdgpu_device_handle dev, unsigned type,
			    unsigned ip_instance,
			    struct drm_amdgpu_info_hw_ip *info);




/**
 * Query heap information
 *
 * This query allows UMD to query potentially available memory resources and
 * adjust their logic if necessary.
 *
 * \param   dev  - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   heap - \c [in] Heap type
 * \param   info - \c [in] Pointer to structure to get needed information
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_query_heap_info(amdgpu_device_handle dev,
			    uint32_t heap,
				uint32_t flags,
			    struct amdgpu_heap_info *info);



/**
 * Get the CRTC ID from the mode object ID
 *
 * \param   dev    - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   id     - \c [in] Mode object ID
 * \param   result - \c [in] Pointer to the CRTC ID
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_query_crtc_from_id(amdgpu_device_handle dev, unsigned id,
			      int32_t *result);



/**
 * Query GPU H/w Info
 *
 * Query hardware specific information
 *
 * \param   dev  - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   heap - \c [in] Heap type
 * \param   info - \c [in] Pointer to structure to get needed information
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX Error code
 *
*/
int amdgpu_query_gpu_info(amdgpu_device_handle dev,
			   struct amdgpu_gpu_info *info);



/**
 * Query hardware or driver information.
 *
 * The return size is query-specific and depends on the "info_id" parameter.
 * No more than "size" bytes is returned.
 *
 * \param   dev     - \c [in] Device handle. See #amdgpu_device_initialize()
 * \param   info_id - \c [in] AMDGPU_INFO_*
 * \param   size    - \c [in] Size of the returned value.
 * \param   value   - \c [out] Pointer to the return value.
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX error code
 *
*/
int amdgpu_query_info(amdgpu_device_handle dev, unsigned info_id,
		      unsigned size, void *value);



/**
 * Read a set of consecutive memory-mapped registers.
 * Not all registers are allowed to be read by userspace.
 *
 * \param   dev          - \c [in] Device handle. See #amdgpu_device_initialize(
 * \param   dword_offset - \c [in] Register offset in dwords
 * \param   count        - \c [in] The number of registers to read starting
 *                                 from the offset
 * \param   instance     - \c [in] GRBM_GFX_INDEX selector. It may have other
 *                                 uses. Set it to 0xffffffff if unsure.
 * \param   flags        - \c [in] Flags with additional information.
 * \param   values       - \c [out] The pointer to return values.
 *
 * \return   0 on success\n
 *          >0 - AMD specific error code\n
 *          <0 - Negative POSIX error code
 *
*/
int amdgpu_read_mm_registers(amdgpu_device_handle dev, unsigned dword_offset,
			     unsigned count, uint32_t instance, uint32_t flags,
			     uint32_t *values);



/**
 * Request GPU access to user allocated memory e.g. via "malloc"
 *
 * \param dev - [in] Device handle. See #amdgpu_device_initialize()
 * \param cpu - [in] CPU address of user allocated memory which we
 * want to map to GPU address space (make GPU accessible)
 * (This address must be correctly aligned).
 * \param size - [in] Size of allocation (must be correctly aligned)
 * \param amdgpu_bo_alloc_result - [out] Handle of allocation to be passed as resource
 * on submission and be used in other operations.(e.g. for VA submission)
 * ( Temporally defined amdgpu_bo_alloc_result as parameter for return mc address. )
 *
 *
 * \return 0 on success
 * >0 - AMD specific error code
 * <0 - Negative POSIX Error code
 *
 *
 * \note
 * This call doesn't guarantee that such memory will be persistently
 * "locked" / make non-pageable. The purpose of this call is to provide
 * opportunity for GPU get access to this resource during submission.
 *
 * The maximum amount of memory which could be mapped in this call depends
 * if overcommit is disabled or not. If overcommit is disabled than the max.
 * amount of memory to be pinned will be limited by left "free" size in total
 * amount of memory which could be locked simultaneously ("GART" size).
 *
 * Supported (theoretical) max. size of mapping is restricted only by
 * "GART" size.
 *
 * It is responsibility of caller to correctly specify access rights
 * on VA assignment.
*/
int amdgpu_create_bo_from_user_mem(amdgpu_device_handle dev,
				    void *cpu,
				    uint64_t size,
				    struct amdgpu_bo_alloc_result *info);


#endif /* #ifdef _AMDGPU_H_ */
