import { GPUAdapter, GPUDevice, GPU } from "../src/framework/gpu/interface.js";

class MyRenderer {
  gpu: GPU;
  rendering: boolean;
  adapter: GPUAdapter | null;
  device: GPUDevice | null;

  constructor(gpu: GPU) {
    this.gpu = gpu;
    this.rendering = false;
    this.adapter = null;
    this.device = null;
  }

  async begin() {
    try {
      await this.initWebGPU();
    } catch (e) {
      console.error(e);
      this.initFallback();
    }
  }

  async initWebGPU() {
    await this.ensureDevice();
    // ... Upload resources, etc.
  }

  initFallback() { /* ... */ }

  async ensureDevice() {
    // Stop rendering. (If there was already a device, WebGPU calls made before
    // the app notices the device is lost are okay - they are no-ops.)
    this.rendering = false;
    if (!this.adapter) {
      // If no adapter, get one.
      // (If requestAdapter rejects, no matching adapter is available. Exit to fallback.)
      this.adapter = await this.gpu.requestAdapter({ /* options */ });
    }
    try {
      this.device = await this.adapter.requestDevice({ /* options */ }, (info) => {
        // Device was lost.
        console.error("device lost", info);
        // Try to get a device again.
        this.ensureDevice();
      });
      this.rendering = true;
    } catch (e) {
      // Request failed (likely due to adapter loss).
      console.error("device request failed", e);
      // Try again with a new adapter.
      this.adapter = null;
      await this.ensureDevice()
    }
  }
}
