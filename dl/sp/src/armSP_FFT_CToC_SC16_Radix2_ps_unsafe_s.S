;//
;// 
;// File Name:  armSP_FFT_CToC_SC16_Radix2_ps_unsafe_s.s
;// OpenMAX DL: v1.0.2
;// Last Modified Revision:   6740
;// Last Modified Date:       Wed, 18 Jul 2007
;// 
;// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
;// 
;// 
;//
;// Description:
;// Compute a Radix 2 FFT stage for a N point complex signal
;// 
;// 

        
;// Include standard headers

        INCLUDE omxtypes_s.h
        INCLUDE armCOMM_s.h
        
        M_VARIANTS CortexA8
        
;// Import symbols required from other files
;// (For example tables)
    
        
        
        
;// Set debugging level        
;//DEBUG_ON    SETL {TRUE}



            
;// Guarding implementation by the processor name
    
    IF  CortexA8 
    
;//Input Registers

pSrc            RN  0
pDst            RN  2
pTwiddle        RN  1
subFFTNum       RN  6
subFFTSize      RN  7


;//Output Registers


;//Local Scratch Registers

outPointStep     RN  3
grpCount         RN  4
dstStep          RN  5
twStep           RN  8
pTmp             RN  4

;// Neon Registers

dW1S32          DN  D0.S32
dW2S32          DN  D1.S32
dW1             DN  D0.S16
dW2             DN  D1.S16

dX0             DN  D2.S16
dX1             DN  D3.S16
dX2             DN  D4.S16
dX3             DN  D5.S16
dY0             DN  D6.S16
dY1             DN  D7.S16
dY2             DN  D8.S16
dY3             DN  D9.S16
qT0             QN  Q5.S32
qT1             QN  Q6.S32


        MACRO
        FFTSTAGE $scaled, $inverse, $name
        
        ;// Define stack arguments
        
        
        ;// Update grpCount and grpSize rightaway inorder to reuse pGrpCount and pGrpSize regs
        
        
        LSL     grpCount,subFFTSize,#1
        
        
        ;// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount
        
        ;// pOut0+1 increments pOut0 by 8 bytes
        ;// pOut0+outPointStep == increment of 4*outPointStep bytes = 2*size bytes
        SMULBB  outPointStep,grpCount,subFFTNum  
        MOV     twStep,subFFTNum,LSL #1
        LSR     subFFTNum,subFFTNum,#1                      ;//grpSize
                                       
                
        RSB      dstStep,outPointStep,#8
                
        
        ;// Note: pointStep is 8 in this case: so need of extra reg
        ;// Loop on the groups: 2 groups at a time

grpLoop$name        
        
        VLD1     dW1S32[],[pTwiddle],twStep                ;//[wi | wr] 
        VLD1     dW2S32[],[pTwiddle],twStep
        
        ;// Process the sets for each grp:  2 sets at a time (no set looping required)     
        
        VLD1    dX0,[pSrc]!            ;// point0: of set0,set1 of grp0
        VLD1    dX1,[pSrc]!            ;// point1: of set0,set1 of grp0
        VLD1    dX2,[pSrc]!            ;// point0: of set0,set1 of grp1
        VLD1    dX3,[pSrc]!            ;// point1: of set0,set1 of grp1
        
        SUBS    grpCount,grpCount,#4              ;// decrement the loop counter
        VUZP    dW1,dW2
        VUZP    dX1,dX3
        
        IF  $inverse
            VMULL   qT0,dX1,dW1
            VMLAL   qT0,dX3,dW2                       ;// real part
            VMULL   qT1,dX3,dW1
            VMLSL   qT1,dX1,dW2                       ;// imag part
            
        ELSE
            VMULL   qT0,dX1,dW1
            VMLSL   qT0,dX3,dW2                       ;// real part
            VMULL   qT1,dX3,dW1
            VMLAL   qT1,dX1,dW2                       ;// imag part
        
        ENDIF
        
        VRSHRN  dX1,qT0,#15
        VRSHRN  dX3,qT1,#15
        
        VZIP    dX1,dX3
        
        
        IF $scaled
        
            VHSUB    dY0,dX0,dX1
            VHADD    dY1,dX0,dX1
            VHSUB    dY2,dX2,dX3
            VHADD    dY3,dX2,dX3
            
        ELSE
        
            VSUB    dY0,dX0,dX1
            VADD    dY1,dX0,dX1
            VSUB    dY2,dX2,dX3
            VADD    dY3,dX2,dX3
            
        
         
        ENDIF
        
        VST1    dY0,[pDst],outPointStep             ;// point0: of set0,set1 of grp0
        VST1    dY1,[pDst],dstStep                  ;// dstStep = -outPointStep + 8
        VST1    dY2,[pDst],outPointStep             ;// point0: of set0,set1 of grp1
        VST1    dY3,[pDst],dstStep                  ;// point1: of set0,set1 of grp1
        
               
        BGT     grpLoop$name
        
        
        ;// Reset and Swap pSrc and pDst for the next stage     
        MOV     pTmp,pDst
        SUB     pDst,pSrc,outPointStep,LSL #1       ;// pDst -= 2*size; pSrc -= 4*size bytes           
        SUB     pSrc,pTmp,outPointStep
        
        ;// Reset pTwiddle for the next stage
        SUB     pTwiddle,pTwiddle,outPointStep      ;// pTwiddle -= 2*size bytes
                
        MEND
        
        
                
        M_START armSP_FFTFwd_CToC_SC16_Radix2_ps_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{FALSE},FWD
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC16_Radix2_ps_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{TRUE},INV
        M_END
 
        
        
        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{FALSE},FWDSFS
        M_END

        
        
        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix2_ps_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{TRUE},INVSFS
        M_END

        
    ENDIF                                                           ;//CORTEXA8
    
     
    END